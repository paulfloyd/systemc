/*****************************************************************************

  The following code is derived, directly or indirectly, from the SystemC
  source code Copyright (c) 1996-2008 by all Contributors.
  All Rights reserved.

  The contents of this file are subject to the restrictions and limitations
  set forth in the SystemC Open Source License Version 2.4 (the "License");
  You may not use this file except in compliance with such restrictions and
  limitations. You may obtain instructions on how to receive a copy of the
  License at http://www.systemc.org/. Software distributed by Contributors
  under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
  ANY KIND, either express or implied. See the License for the specific
  language governing rights and limitations under the License.

 *****************************************************************************/

/*****************************************************************************

  sc_reset.cpp -- Support for reset.

  Original Author: Andy Goodrich, Forte Design Systems

 *****************************************************************************/

// $Log: sc_reset.cpp,v $
// Revision 1.4  2009/05/22 16:06:29  acg
//  Andy Goodrich: process control updates.
//
// Revision 1.3  2009/03/12 22:59:58  acg
//  Andy Goodrich: updates for 2.4 stuff.
//
// Revision 1.2  2008/05/22 17:06:26  acg
//  Andy Goodrich: updated copyright notice to include 2008.
//
// Revision 1.1.1.1  2006/12/15 20:20:05  acg
// SystemC 2.3
//
// Revision 1.7  2006/12/02 20:58:19  acg
//  Andy Goodrich: updates from 2.2 for IEEE 1666 support.
//
// Revision 1.5  2006/04/11 23:13:21  acg
//   Andy Goodrich: Changes for reduced reset support that only includes
//   sc_cthread, but has preliminary hooks for expanding to method and thread
//   processes also.
//
// Revision 1.4  2006/01/24 20:49:05  acg
// Andy Goodrich: changes to remove the use of deprecated features within the
// simulator, and to issue warning messages when deprecated features are used.
//
// Revision 1.3  2006/01/13 18:44:30  acg
// Added $Log to record CVS changes into the source.
//

#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_reset.h"
#include "sysc/kernel/sc_process_handle.h"
#include "sysc/communication/sc_signal.h"
#include "sysc/communication/sc_signal_ports.h"


namespace sc_core {

class sc_reset_finder;
static sc_reset_finder* reset_finder_q=0;  // Q of reset finders to reconcile.

//==============================================================================
// sc_reset_finder -
//
//==============================================================================
class sc_reset_finder {
    friend class sc_reset;
  public:
    sc_reset_finder( bool async, const sc_in<bool>* port_p, bool level, 
        sc_process_b* target_p);
    sc_reset_finder( bool async, const sc_inout<bool>* port_p, bool level, 
        sc_process_b* target_p);
    sc_reset_finder( bool async, const sc_out<bool>* port_p, bool level, 
        sc_process_b* target_p);

  protected:
    bool                   m_async;     // True if asynchronous reset.
    bool                   m_level;     // Level for reset.
    sc_reset_finder*       m_next_p;    // Next reset finder in list.
    const sc_in<bool>*     m_in_p;      // Port for which reset is needed.
    const sc_inout<bool>*  m_inout_p;   // Port for which reset is needed.
    const sc_out<bool>*    m_out_p;     // Port for which reset is needed.
    sc_process_b*          m_target_p;  // Process to reset.

  private: // disabled
    sc_reset_finder( const sc_reset_finder& );
    const sc_reset_finder& operator = ( const sc_reset_finder& );
};

inline sc_reset_finder::sc_reset_finder(
    bool async, const sc_in<bool>* port_p, bool level, sc_process_b* target_p
) : 
    m_async(async), m_level(level), m_in_p(port_p), m_inout_p(0), m_out_p(0), 
    m_target_p(target_p)
{   
    m_next_p = reset_finder_q;
    reset_finder_q = this;
}

inline sc_reset_finder::sc_reset_finder(
    bool async, const sc_inout<bool>* port_p, bool level, sc_process_b* target_p
) : 
    m_async(async), m_level(level), m_in_p(0), m_inout_p(port_p), m_out_p(0), 
    m_target_p(target_p)
{   
    m_next_p = reset_finder_q;
    reset_finder_q = this;
}

inline sc_reset_finder::sc_reset_finder(
    bool async, const sc_out<bool>* port_p, bool level, sc_process_b* target_p
) : 
    m_async(async), m_level(level), m_in_p(0), m_inout_p(0), m_out_p(port_p), 
    m_target_p(target_p)
{   
    m_next_p = reset_finder_q;
    reset_finder_q = this;
}


//------------------------------------------------------------------------------
//"sc_reset::notify_processes"
//
// Notify processes that there is a change in the reset signal value.
//------------------------------------------------------------------------------
void sc_reset::notify_processes()
{
    bool             active;       // true if reset is active.
    sc_reset_target* entry_p;      // reset entry processing.
    int              process_i;    // index of process resetting.
    int              process_n;    // # of processes to reset.
    bool             value;        // value of our signal.

    value = m_iface_p->read();
    process_n = m_targets.size();
    for ( process_i = 0; process_i < process_n; process_i++ )
    {
        entry_p = &m_targets[process_i];
	active = ( entry_p->m_level == value );
	entry_p->m_process_p->reset_changed( entry_p->m_async, active );
    }
}


//------------------------------------------------------------------------------
//"sc_reset::reconcile_resets"
//
// This static method processes the sc_reset_finders to establish the actual
// reset connections.
//
// Notes:
//   (1) If reset is asserted we tell the process that it is in reset
//       initially.
//------------------------------------------------------------------------------
void sc_reset::reconcile_resets()
{
    const sc_signal_in_if<bool>*  iface_p;      // Interface to reset signal.
    sc_reset_finder*              next_p;       // Next finder to process.
    sc_reset_finder*              now_p;        // Finder currently processing.
    sc_reset_target               reset_target; // Target's reset entry.
    sc_reset*                     reset_p;      // Reset object to use.

    for ( now_p = reset_finder_q; now_p; now_p = next_p )
    {
        next_p = now_p->m_next_p;
#if 0 // @@@@ REMOVE
        if ( now_p->m_target_p->m_reset_p || now_p->m_target_p->m_areset_p )
            SC_REPORT_ERROR(SC_ID_MULTIPLE_RESETS_,now_p->m_target_p->name());
#endif
        if ( now_p->m_in_p )
        {
            iface_p = DCAST<const sc_signal_in_if<bool>*>(
                now_p->m_in_p->get_interface());
        }
        else if ( now_p->m_inout_p )
        {
            iface_p = DCAST<const sc_signal_in_if<bool>*>(
                now_p->m_inout_p->get_interface());
        }
        else
        {
            iface_p = DCAST<const sc_signal_in_if<bool>*>(
                now_p->m_out_p->get_interface());
        }
        assert( iface_p != 0 );
        reset_p = iface_p->is_reset();
	now_p->m_target_p->m_resets.push_back(reset_p);
	reset_target.m_async = now_p->m_async;
	reset_target.m_level = now_p->m_level;
	reset_target.m_process_p = now_p->m_target_p;
	reset_p->m_targets.push_back(reset_target);
	if ( iface_p->read() == now_p->m_level ) // see note 1 above
	    now_p->m_target_p->initially_in_reset( now_p->m_async );
        delete now_p;
    }
}


//------------------------------------------------------------------------------
//"sc_reset::remove_process"
//
//------------------------------------------------------------------------------
void sc_reset::remove_process( sc_process_b* process_p )
{
    int process_i; // Index of process resetting.
    int process_n; // # of processes to reset.

    process_n = m_targets.size();
    for ( process_i = 0; process_i < process_n; )
    {
        if ( m_targets[process_i].m_process_p == process_p )
        {
            m_targets[process_i] = m_targets[process_n-1];
	    process_n--;
            m_targets.resize(process_n);
        }
	else
	{
	    process_i++;
	}
    }
}

//------------------------------------------------------------------------------
//"sc_reset::reset_signal_is"
//
//------------------------------------------------------------------------------
void sc_reset::reset_signal_is(
    bool async, const sc_in<bool>& port, bool level)
{
    const sc_signal_in_if<bool>* iface_p;
    sc_process_b*                process_p;
    
    process_p = (sc_process_b*)sc_get_current_process_handle();
    assert( process_p );
    switch ( process_p->proc_kind() )
    {
      case SC_THREAD_PROC_:
      case SC_METHOD_PROC_:
      case SC_CTHREAD_PROC_:
        iface_p = DCAST<const sc_signal_in_if<bool>*>(port.get_interface());
        if ( iface_p )
            reset_signal_is( async, *iface_p, level );
        else
        {
            new sc_reset_finder( async, &port, level, process_p );
        }
        break;
      default:
        SC_REPORT_ERROR(SC_ID_UNKNOWN_PROCESS_TYPE_, process_p->name());
        break;
    }
}

void sc_reset::reset_signal_is(
    bool async, const sc_inout<bool>& port, bool level )
{
    const sc_signal_in_if<bool>* iface_p;
    sc_process_b*                process_p;
    
    process_p = (sc_process_b*)sc_get_current_process_handle();
    assert( process_p );
    switch ( process_p->proc_kind() )
    {
      case SC_THREAD_PROC_:
      case SC_METHOD_PROC_:
      case SC_CTHREAD_PROC_:
        // @@@@ CAN THIS GO? process_p->m_reset_level = level;
        iface_p = DCAST<const sc_signal_in_if<bool>*>(port.get_interface());
        if ( iface_p )
            reset_signal_is( async, *iface_p, level );
        else
        {
            new sc_reset_finder( async, &port, level, process_p );
        }
        break;
      default:
        SC_REPORT_ERROR(SC_ID_UNKNOWN_PROCESS_TYPE_, process_p->name());
        break;
    }
}

void sc_reset::reset_signal_is(
    bool async, const sc_out<bool>& port, bool level )
{
    const sc_signal_in_if<bool>* iface_p;
    sc_process_b*                process_p;
    
    process_p = (sc_process_b*)sc_get_current_process_handle();
    assert( process_p );
    switch ( process_p->proc_kind() )
    {
      case SC_THREAD_PROC_:
      case SC_METHOD_PROC_:
      case SC_CTHREAD_PROC_:
        // @@@@ process_p->m_reset_level = level;
        iface_p = DCAST<const sc_signal_in_if<bool>*>(port.get_interface());
        if ( iface_p )
            reset_signal_is( async, *iface_p, level );
        else
        {
            new sc_reset_finder( async, &port, level, process_p );
        }
        break;
      default:
        SC_REPORT_ERROR(SC_ID_UNKNOWN_PROCESS_TYPE_, process_p->name());
        break;
    }
}

//------------------------------------------------------------------------------
//"sc_reset::reset_signal_is"
//
// Notes:
//   (1) If reset is asserted we tell the process that it is in reset
//       initially.
//------------------------------------------------------------------------------
void sc_reset::reset_signal_is( 
    bool async, const sc_signal_in_if<bool>& iface, bool level )
{
    sc_process_b*   process_p;    // process adding reset for.
    sc_reset_target reset_target; // entry to build for the process.
    sc_reset*       reset_p;      // reset object.

    process_p = sc_process_b::last_created_process_base();
    assert( process_p );
#if 0 // @@@@ REMOVE OR CONDITIONAL ON SYSC 2.2
    if ( process_p->m_reset_p )
        SC_REPORT_ERROR(SC_ID_MULTIPLE_RESETS_,process_p->name());
#endif
    switch ( process_p->proc_kind() )
    {
      case SC_METHOD_PROC_:
        if ( !async ) {
            // @@@@SC_REPORT_ERROR(SC_ID_NO_METHOD_SYNC_RESET_,process_p->name());
	    break;
	} // fall through...
      case SC_CTHREAD_PROC_:
      case SC_THREAD_PROC_:
	reset_p = iface.is_reset();
	process_p->m_resets.push_back(reset_p);
        reset_target.m_async = async; 
	reset_target.m_level = level;
	reset_target.m_process_p = process_p;
	reset_p->m_targets.push_back(reset_target);
	if ( iface.read() == level ) process_p->initially_in_reset( async );
        break;
      default:
        SC_REPORT_ERROR(SC_ID_UNKNOWN_PROCESS_TYPE_, process_p->name());
        break;
    }
}


} // namespace sc_core