///////////////////////////////////////////////////////////////////////////////
//
// Filename:	div_tb.cpp
//
// Project:	Zip CPU -- a small, lightweight, RISC CPU soft core
//
// Purpose:	Bench testing for the divide unit found within the Zip CPU.
//
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015, Gisselquist Technology, LLC
//
// This program is free software (firmware): you can redistribute it and/or
// modify it under the terms of  the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// License:	GPL, v3, as defined and found on www.gnu.org,
//		http://www.gnu.org/licenses/gpl.html
//
//
///////////////////////////////////////////////////////////////////////////////
//
//
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include <ctype.h>

#include "verilated.h"
#include "Vdiv.h"

#include "testb.h"
// #include "twoc.h"

class	DIV_TB : public TESTB<Vdiv> {
public:
	DIV_TB(void) {
	}

	~DIV_TB(void) {}

	void	reset(void) {
		// m_flash.debug(false);
		TESTB<Vdiv>::reset();
	}

	bool	on_tick(void) {
		tick();
		return true;
	}

	void	bprint(char *str, int nbits, unsigned long v) {
		while(*str)
			str++;
		for(int i=0; i<nbits; i++) {
			if ((1l<<(nbits-1-i))&v)
				*str++ = '1';
			else
				*str++ = '0';
			if (((nbits-1-i)&3)==0)
				*str++ = ' ';
		} *str = '\0';
	}

	void	dbgdump(void) {
		char	outstr[2048], *s;
		sprintf(outstr, "Tick %4ld %s%s%s%s%s%s%s %2d(%s= 0)",
			m_tickcount,
			(m_core->o_busy)?"B":" ",
			(m_core->v__DOT__r_busy)?"R":" ",
			(m_core->o_valid)?"V":" ",
			(m_core->i_wr)?"W":" ",
			(m_core->v__DOT__pre_sign)?"+":" ",
			(m_core->v__DOT__r_sign)?"-":" ",
			(m_core->v__DOT__r_z)?"Z":" ",
			m_core->v__DOT__r_bit,
			(m_core->v__DOT__last_bit)?"=":"!");
		s = &outstr[strlen(outstr)];
		sprintf(s, "%s\n%10s %40s",s, "Div","");
			s = &s[strlen(s)];
		bprint( s, 32, m_core->v__DOT__r_dividend);
			s=&s[strlen(s)];
		sprintf(s, "%s\n%10s ",s, "Div"); s = &s[strlen(s)];
		bprint( s, 64, m_core->v__DOT__r_divisor);
			s=&s[strlen(s)];
		sprintf(s, "%s\n%10s %40s",s, "Q",""); s=&s[strlen(s)];
		bprint( s, 32, m_core->o_quotient); s = &s[strlen(s)];
		sprintf(s, "%s\n%10s %38s",s, "Diff","");
			s=&s[strlen(s)];
		bprint( s, 33, m_core->v__DOT__diff); s = &s[strlen(s)];
		strcat(s, "\n");
		puts(outstr);
	}

	void	tick(void) {
		bool	debug = false;

		if (debug)
			dbgdump();
		TESTB<Vdiv>::tick();
	}

	void	divtest(uint32_t n, uint32_t d, uint32_t ans, bool issigned) {
		const bool	dbg = false;

		// The test bench is supposed to assert that we are idle when
		// we come in here.
		assert(m_core->o_busy == 0);

		// Request a divide
		m_core->i_rst = 0;
		m_core->i_wr = 1;
		m_core->i_signed = (issigned)?1:0;
		m_core->i_numerator = n;
		m_core->i_denominator = d;

		// Tick once for the request to be registered
		tick();

		// Clear the input lines.
		m_core->i_wr = 0;
		m_core->i_signed = 0;
		m_core->i_numerator = 0;
		m_core->i_denominator = 0;

		// Make certain busy is immediately true upon the first clock
		// after we issue the divide, and that our result is not also
		// listed as a valid result.
		if (!m_core->o_busy) {
			closetrace();
			assert(m_core->o_busy);
		} if (m_core->o_valid != 0) {
			closetrace();
			assert(m_core->o_valid == 0);
		}

		// while((!m_core->o_valid)&&(!m_core->o_err))
		while(!m_core->o_valid) {
			// If we aren't yet valid, we'd better at least
			// be busy--the CPU requires this.
			if (!m_core->o_busy) {
				// We aren't valid, and we aren't busy.  This
				// is a test failure.
				dbgdump();
				closetrace();
				assert(m_core->o_busy);
			}

			// Let the algorithm work for another clock tick.
			tick();
		} if (dbg) dbgdump();

		// Insist that the core not be busy any more, now that a valid
		// result has been produced.
		if (m_core->o_busy) {
			closetrace();
			assert(!m_core->o_busy);
		}

		if (dbg) {
			printf("%s%s: %d / %d =? %d\n",
				(m_core->o_valid)?"V":" ",
				(m_core->o_err)?"E":" ",
				n, d, m_core->o_quotient);
		}


		// Now that we're done, we need to check the result.
		//
		// First case to check: was there an error condition or, if not,
		// should there have been one?
		if (d == 0) {
			// We attempted to divide by zero, the result should've
			// been an error condition.  Let's check:
			// Then insist on a division by zero error
			if (!m_core->o_err) {
				// Don't forget to close the trace before the
				// assert, lest the file not get the final
				// values into it.
				closetrace();
				assert(m_core->o_err);
			}
		} else if (m_core->o_err) {
			// Otherwise, there should not have been any divide
			// errors.  The only errors allowed should be the
			// divide by zero.  So, this is an error.  Let's
			// stop and report it.
			closetrace();
			assert(!m_core->o_err);
		} else if (ans != (uint32_t)m_core->o_quotient) {
			// The other problem we might encounter would be if the
			// result doesn't match the one we are expecting.
			//
			// Stop on this bug as well.
			//
			closetrace();
			assert(ans == (uint32_t)m_core->o_quotient);
		}
	}

	// Test a signed divide
	void	divs(int n, int d) {
		int	ans;
		// Calculate the answer we *should* get from the divide
		ans = (d==0)?0:	(n / d);

		divtest((uint32_t)n, (uint32_t)d, (uint32_t)ans, true);
	}

	// Test an unsigned divide
	void	divu(unsigned n, unsigned d) {
		unsigned	ans;

		// Pre-Calculate the answer we *should* get from the divide
		ans = (d==0)?0:	(n / d);

		divtest((uint32_t)n, (uint32_t)d, (uint32_t)ans, false);
	}

	// divide() is just another name for a signed divide--just switch to
	// that function call instead.
	void	divide(int n, int d) {
		divs(n,d);
	}
};

//
// Standard usage functions.
//
// Notice that the test bench provides no options.  Everything is
// self-contained.
void	usage(void) {
	printf("USAGE: div_tb\n");
	printf("\n");
	printf("\t\n");
}

//
int	main(int argc, char **argv) {
	// Setup
	Verilated::commandArgs(argc, argv);
	DIV_TB	*tb = new DIV_TB();

	tb->reset();
	// tb->opentrace("div_tb.vcd");

	// Now we're ready.  All we need to do to test the divide of two
	// numbers is to call the respective divide(), divs(), or divu()
	// functions.  The program will crash on an assert error if anything
	// goes wrong.
	tb->divide(125,7);
	// And give us an extra clock tick in-between each test for good
	// measure.
	tb->tick();

	// Some other gentle tests
	tb->divide(125,-7);
	tb->tick();
	tb->divu((1u<<31),7);
	// Now some boundary conditions
	tb->divu((7u<<29),(1u<<31));
	tb->tick();
	tb->divs(32768,0);
	tb->tick();
	tb->divu((1u<<31),0);
	tb->tick();
	tb->divs((1u<<30),0);
	tb->tick();
	//
	// Now we switch to a more thorough test set.  It's not complete, just
	// ... more thorough.
	for(int i=32767; i>=0; i--) {
		tb->divs((1u<<30),i);
		tb->tick();
	} for(int i=32767; i>=0; i--) {
		// tb->divu(-1, i);
		tb->divu((1u<<31), i);
		tb->tick();
	} for(int i=32767; i>=0; i--) {
		tb->divide(32768, i);
		tb->tick();
	}

	/*
	 * While random data is a nice test idea, the following just never
	 * really tested the divide unit thoroughly enough.
	 *
	tb->divide(rand(),rand()/2);
	tb->tick();
	tb->divide(rand(),rand()/2);
	tb->tick();
	tb->divide(rand(),rand()/2);
	tb->tick();
	tb->divide(rand(),rand()/2);
	*/

	// Any failures above will be captured with a failed assert.  If we
	// get here, it means things worked.  Close up shop ...
	//
	// This closes any potential trace file
	delete	tb;

	// And declare success
	printf("SUCCESS!\n");
	exit(EXIT_SUCCESS);
}

