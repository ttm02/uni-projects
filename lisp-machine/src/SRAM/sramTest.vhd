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
entity memTst is
generic(clkPeriod	: time		:= 20 ns;	-- clock period
	clkCycles	: positive	:= 10);		-- clock cycles
end entity memTst;


-- architecture	--------------------------------------------------------------
------------------------------------------------------------------------------
architecture testbench of memTst is
  signal clk, nRst	: std_logic;
  signal const0, const1	: std_logic;
  signal nWE 	: std_logic;
  signal RAddr, WAddr	: std_logic_vector( 7 downto 0);
  signal iData: std_logic_vector(31 downto 0);
  signal Page: std_logic_vector(127 downto 0);
  signal iCtrl	: fileIOty;

begin
  const0 <= '0';
  const1 <= '1';

  -- memories		------------------------------------------------------
  instMemI: sram	generic map (	addrWd	=> 8,
					dataWd	=> 32,
					fileID	=> "/informatik2/students/home/3jammer/projekt-mikrocontroller/src/SRAM/TestMem.dat")
			port map    (	clk =>clk,
					nCS	=> const0,
					nWE	=> nWe,
					Read_addr	=> RAddr,
					dataIn	=> iData,
					Write_addr	=> WAddr,
					dataOut	=> Page,

					fileIO	=> iCtrl);
  
    -- stimuli		------------------------------------------------------
  stiP: process is
  begin
    clk		<= '0';
    nRst	<= '0',   '1'  after 5 ns;
    iCtrl	<= load,  none after 5 ns;
	nWE<='1';

    
	
	Raddr <= "00000000";
	WAddr <= "00000011";
	idata <= "11111111111111111111111111111111";
    wait for clkPeriod/2;
	clk <= '0';
	nWE<='0';
    wait for clkPeriod/2;
clk<= '1';
    wait for clkPeriod/2;
clk <= '0';
nWE <= '1';
    wait for clkPeriod/2;
clk<= '1';
    wait for clkPeriod/2;
clk <= '0';

Raddr <= "00000100";
   wait for clkPeriod/2;
clk<= '1';

 iCtrl	<= dump,  none after 5 ns;


    for n in 1 to clkCycles loop
	clk <= '0', '1' after clkPeriod/2;


	wait for clkPeriod;
    end loop;
    wait;
  end process stiP;

end architecture testbench;
------------------------------------------------------------------------------
-- procTst.vhd	- end
