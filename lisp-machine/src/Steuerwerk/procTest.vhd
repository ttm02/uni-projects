-- procTst.vhd
--
-- entity	procTst			-testbench for pipeline processor
-- architecture	testbench		-
------------------------------------------------------------------------------
library ieee;						-- packages:
use ieee.std_logic_1164.all;				--   std_logic
use ieee.numeric_std.all;				--   (un)signed
use work.sramPkg.all;					--   sram
--use work.procPkg.all;					--   pipeProc

-- entity	--------------------------------------------------------------
------------------------------------------------------------------------------
entity ProcTst is
generic(clkPeriod	: time		:= 20 ns;	-- clock period
	clkCycles	: positive	:= 100000);		-- clock cycles
end entity ProcTst;


-- architecture	--------------------------------------------------------------
------------------------------------------------------------------------------
architecture testbench of ProcTst is
  signal clk : std_logic;

  signal fileIO	: fileIOty;

signal Memaddr: std_logic_vector (31 downto 0); 
signal MemWrite: std_logic_vector (127 downto 0);
signal MemRead: std_logic_vector (127 downto 0);

signal MemWenable: std_logic;
signal NMemWenable: std_logic;
 
signal nCs : std_logic := '0';

begin


  instControll: entity work.controller	
			port map    (	clk =>clk,
					Memaddr => Memaddr,
					MemWrite => MemWrite,
					MemRead => MemRead,
					MemWenable => MemWenable

				);
  -- memories		------------------------------------------------------
--instanziiere Ram
MEM: entity work.sram	generic map (	addrWd	=> 8,
					dataWd	=> 128,
					fileID	=> "/home/tim/projekt-mikrocontroller/src/TestMem.dat")
-- "/informatik2/students/home/3jammer/projekt-mikrocontroller/src/TestMem.dat"
			port map    (	clk =>clk,
					nCS	=> nCS,
					nWE	=> NMemWenable,
					Read_addr	=> Memaddr(7 downto 0),
					dataIn	=> MemWrite,
					Write_addr	=> Memaddr(7 downto 0),
					dataOut	=> MemRead,
					fileIO	=> fileIO);

  
    -- stimuli		------------------------------------------------------
  stiP: process is
  begin
    clk		<= '0';
      fileIO	<= load,  none after 5 ns;

    for n in 1 to clkCycles loop
	clk <= '0', '1' after clkPeriod/2;
	wait for clkPeriod;
    end loop;

fileIO	<= dump,  none after 5 ns;
    wait;
  end process stiP;

wenable: process(MemWenable)
begin
NMemWenable <= not MemWenable;
end process wenable;





end architecture testbench;
------------------------------------------------------------------------------
-- procTst.vhd	- end
