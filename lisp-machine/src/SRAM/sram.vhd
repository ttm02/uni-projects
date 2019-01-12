-- sram.vhd


------------------------------------------------------------------------------
-- sramPkg		------------------------------------------------------
------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
package sramPkg is
  type fileIOty	is (none, dump, load);

  component sram is
  generic (	addrWd	: integer range 2 to 32	:= 8;	-- #address bits
		dataWd	: integer range 2 to 128:= 8;	-- #data    bits
		fileId	: string		:= "sram.dat"); -- filename
port (        	clk	: in 	std_logic; --clk
		nCS    : in     std_logic;           -- not Chip   Select
                nWE    : in     std_logic;           -- not Write  Enable
                Write_addr   : in     std_logic_vector(addrWd-1 downto 0);
		dataIn   : in  std_logic_vector(dataWd-1 downto 0);

		Read_addr   : in     std_logic_vector(addrWd-1 downto 0);
     		dataOUT :out std_logic_vector(dataWd-1 downto 0);-- one Data-Page out
                fileIO : in     fileIOty       := none);

  end component sram;
end package sramPkg;
------------------------------------------------------------------------------
-- sram			------------------------------------------------------
------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use std.textio.all;
use ieee.std_logic_textio.all;
use work.sramPkg.all;

entity sram is
generic (	addrWd	: integer range 2 to 32	:= 8;	-- #address bits
		dataWd	: integer range 2 to 128	:= 8;	-- #data    bits
		fileId	: string		:= "sram.dat"); -- filename
port (        	clk	: in 	std_logic; --clk
		nCS    : in     std_logic;           -- not Chip   Select
                nWE    : in     std_logic;           -- not Write  Enable
                Write_addr   : in     std_logic_vector(addrWd-1 downto 0);
		dataIn   : in  std_logic_vector(dataWd-1 downto 0);

		Read_addr   : in     std_logic_vector(addrWd-1 downto 0);
     		dataOUT :out std_logic_vector(dataWd-1 downto 0);-- one Data-Page out
                fileIO : in     fileIOty       := none);
end entity sram;

-- sram(simModel)	------------------------------------------------------
------------------------------------------------------------------------------
architecture simModel of sram is
begin

  -- sram		simulation model
  ----------------------------------------------------------------------------
  sramP: process (clk, nCS, nWE, read_addr, write_addr, dataIn, fileIO) is
    constant	addrHi		: natural	:= (2**addrWd)-1;

    subtype	sramEleTy	is std_logic_vector(dataWd-1 downto 0);
    type	sramMemTy	is array (0 to addrHi) of sramEleTy;

    variable	sramMem		:  sramMemTy;

    file	ioFile		: text;
    variable	ioLine		: line;
    variable	ioStat		: file_open_status;
    variable	rdStat		: boolean;
    variable	ioAddr		: integer range sramMem'range;
    variable	ioData		: std_logic_vector(dataWd-1 downto 0);
  begin
    -- fileIO	dump/load the SRAM contents into/from file
    --------------------------------------------------------------------------
    if fileIO'event then
      if fileIO = dump	then	--  dump sramData	----------------------
	file_open(ioStat, ioFile, fileID, write_mode);
	assert ioStat = open_ok
	  report "SRAM - dump: error opening data file"
	  severity error;
	for dAddr in sramMem'range loop
	  write(ioLine, dAddr);			-- format line:
	  write(ioLine, ' ');				--   <addr> <data>
	  write(ioLine, std_logic_vector(sramMem(dAddr)));
	  writeline(ioFile, ioLine);		-- write line
	end loop;
	file_close(ioFile);

      elsif fileIO = load then	--  load sramData	----------------------
	file_open(ioStat, ioFile, fileID, read_mode);
	assert ioStat = open_ok
	  report "SRAM - load: error opening data file"
	  severity error;
	while not endfile(ioFile) loop
	  readline(ioFile, ioLine);			-- read line
	  read(ioLine, ioAddr, rdStat);			-- read <addr>
	  if rdStat then				--      <data>
	    read(ioLine, ioData, rdStat);
	  end if;
	  if rdStat then
	    sramMem(ioAddr) := ioData;
	  else
	    report "SRAM - load: format error in data file"
	    severity error;
	  end if;
	end loop;
	file_close(ioFile);
      end if;	-- fileIO = ...
    end if;	-- fileIO'event

    -- consistency checks
    ------------------------------------------------------------------------
    if nCS'event  then	assert not Is_X(nCS)
			  report "SRAM: nCS - X value"
			  severity warning;
    end if;
    if nWE'event  then	assert not Is_X(nWE)
			  report "SRAM: nWE - X value"
			  severity warning;
    end if;
    --if nOE'event  then	assert not Is_X(nOE)
	--		  report "SRAM: nOE - X value"
	--		  severity warning;
--    end if;
    if Write_addr'event then	assert not Is_X(write_addr)
			  report "SRAM: addr - X value"
			  severity warning;
    end if;
--    if data'event then	assert not Is_X(data)
--			  report "SRAM: data - X value"
--			  severity warning;
--    end if;

    -- here starts the real work...
    ------------------------------------------------------------------------
    --data <= (others => 'Z');				-- output disabled

   if nCS = '0'        then                            -- chip enabled
      if nWE = '0'      then                            -- +write cycle
	if falling_edge(clk) then -- schreiben bei falling edge
        sramMem(to_integer(unsigned(write_addr))) := dataIn;
	end if;
      end if; -- nWE = ...

          -- +read  cycle
        dataOUT <= sramMem(to_integer(unsigned(read_addr)));

    end if;   -- nCS = '0'

  end process sramP;

end architecture simModel;
------------------------------------------------------------------------------
-- sram.vhd - end	------------------------------------------------------
