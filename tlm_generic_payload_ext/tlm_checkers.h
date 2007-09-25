/*****************************************************************************

  The following code is derived, directly or indirectly, from the SystemC
  source code Copyright (c) 1996-2004 by all Contributors.
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

#ifndef _TLM_CHECKERS_H
#define _TLM_CHECKERS_H

#include "tlm_generic_payload.h"

namespace tlm {

class tlm_checker
{
public:

	// constructor (a tlm_checker applies on a certain address space)
	tlm_checker(sc_dt::uint64 start_address_range, // first address 
				sc_dt::uint64 end_address_range,   // last address
				unsigned int bus_data_width)
		: m_start_address_range(start_address_range)
		, m_end_address_range(end_address_range)
		, m_bus_data_width(bus_data_width)
		, m_write_command_supported(true)
		, m_read_command_supported(true)
		, m_burst_supported(true)
		, m_burst_mode_incremental_supported(true)
		, m_burst_mode_streaming_supported(true)
		, m_burst_mode_wrapping_supported(true)
		, m_response_status(TLM_OK_RESP)
	{
	}

	// destructor
	~tlm_checker(){}

	// convenient methods to set the tlm_checker options
	inline void write_command_not_supported() {m_write_command_supported = false;}
	inline void read_command_not_supported() {m_read_command_supported = false;}
	inline void burst_not_supported() {m_burst_supported = false;}
	inline void burst_mode_incremental_not_supported() {m_burst_mode_incremental_supported = false;}
	inline void burst_mode_streaming_not_supported() {m_burst_mode_streaming_supported = false;}
	inline void burst_mode_wrapping_not_supported() {m_burst_mode_wrapping_supported = false;}

	// main function to check if the transaction is valid 
	bool transactionIsValid(tlm_generic_payload* gp)
	{
		m_response_status = do_check(gp);

		return (m_response_status == TLM_OK_RESP);
	}

	tlm_response_status get_response_status() {return m_response_status;}

private:
	
	tlm_response_status do_check(tlm_generic_payload* gp)
	{
		// Check 1: Write transaction supported
		if((gp->get_command() == TLM_WRITE_COMMAND) && (m_write_command_supported == false))
			return TLM_COMMAND_ERROR_RESP;

		// Check 2: Read transacion supported
		if((gp->get_command() == TLM_READ_COMMAND) && (m_read_command_supported == false))
			return TLM_COMMAND_ERROR_RESP;

		// Check 3: burst_data_size (bytes) bigger than bus_data_width (bits)
		if(gp->get_burst_data_size() > (m_bus_data_width/8))
			return TLM_BURST_ERROR_RESP;

        // Check 4: check supported burst
		if(gp->get_burst_length() > 1)
		{
			if(m_burst_supported == false)
			{
				return TLM_BURST_ERROR_RESP;
			}
			else
			{
				if((gp->get_burst_mode() == TLM_INCREMENT_BURST) && m_burst_mode_incremental_supported == false)
					return TLM_BURST_ERROR_RESP;
				if((gp->get_burst_mode() == TLM_STREAMING_BURST) && m_burst_mode_streaming_supported == false)
					return TLM_BURST_ERROR_RESP;
				if((gp->get_burst_mode() == TLM_WRAPPING_BURST) && m_burst_mode_wrapping_supported == false)
					return TLM_BURST_ERROR_RESP;
			}
		}
		
		// Check 5: address has to be aligned on burst_data_size
        if (gp->get_address() % gp->get_burst_data_size() != 0) 
			return TLM_ADDRESS_ERROR_RESP;

		// Check 6: check address within range (in case burst_mode is incremental)
		if(gp->get_burst_mode() == TLM_INCREMENT_BURST)
		{
			unsigned int m_incr_address = m_bus_data_width/8; 
			sc_dt::uint64 begin_address = gp->get_address();
			sc_dt::uint64 end_address = begin_address + gp->get_burst_length()*m_incr_address;
			
			if((begin_address < m_start_address_range) && (end_address > m_end_address_range))
				return TLM_ADDRESS_ERROR_RESP;
		}

		return TLM_OK_RESP;
	}

private:

	unsigned int m_bus_data_width;
	sc_dt::uint64 m_start_address_range;
	sc_dt::uint64 m_end_address_range;
	bool m_write_command_supported;
	bool m_read_command_supported;
	bool m_burst_supported;
	bool m_burst_mode_incremental_supported;
	bool m_burst_mode_streaming_supported;
	bool m_burst_mode_wrapping_supported;

	tlm_response_status m_response_status;

};

}

#endif
