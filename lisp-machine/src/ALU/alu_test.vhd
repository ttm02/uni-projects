library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.std_logic_arith.all;

USE work.constants.all;

entity alu_tb is

end alu_tb;

architecture Behavior of alu_tb is
  -- Inputs and outputs
--  signal Clk : std_logic := '0';
  signal Data1, Data2 , Data_Out, Flags: std_logic_vector(29 downto 0) := (others => '0');
  signal OPcode : std_logic_vector(7 downto 0) := (others => '0');

  signal i: integer := 0; -- schleifenzähler
  constant clk_period : time := 10 ns;
  constant clk_cycles : integer := 1000;

  begin
-- Instanziiere "Unit under test"
    uut: entity work.ALU port map (
      	Data1 => Data1,
	Data2 => Data2,
	Opcode => Opcode,
	Data_Out => Data_Out,
	Flags => Flags

    );

--    Clk_process: process
--    begin 
--	  for i in 0 to clk_cycles loop
--	  	Clk <= '0';
--          	wait for Clk_period / 2;
--          	Clk <= '1';
--          	wait for Clk_period / 2;
--	  end loop;
--	wait;
--    end process;


    alusim_process: process
      begin
        wait for clk_period*3;
        for i in 0 to 255 loop -- Alle Zahlen von 0 bis 255
          Data1 <= conv_std_logic_vector(i,30);
	  Data2 <= conv_std_logic_vector(i,30);
          wait for clk_period;
		OPcode <= opcSUB;
          wait for clk_period;
		OPcode <= opcADD;


        end loop;
        
        wait;
      end process;

    end architecture ;
