-- File: stack_test.vhdl
-- Authors: Sandro, Thomas
--
-- The test bench for the stack. 
-- 


library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.std_logic_arith.all;

use work.constants.all;

entity stack_tb is
end stack_tb;

architecture Beavior of stack_tb is
  -- Inputs and outputs
  signal Clk, Enable, Full, Empty: std_logic := '0';
  signal Mode: std_logic_vector(1 downto 0);
  -- Data_In is the input that should be pushed to the stack
  -- TOS = Top of Stack NXT= Next of Stack
  signal TOS,NXT, Data_IN: std_logic_vector(STACK_ELEMENT_WIDTH downto 0) := (others => '0');
  -- Test data
  signal i: integer := 0;
  -- End signal
  signal clk_stop : std_logic := '0';
  -- How long the simulation should last
  constant clk_period: time := 10 ns;

  begin
    -- Define the port map of the entity
    -- See stack.vhdl 
    uut: entity work.stack port map (
      Clk => Clk,
      Enable => Enable,
      Top_Of_Stack => TOS,
      Next_Of_Stack => NXT,
      Data_In => Data_IN,
      MODE => Mode,
      Stack_Full => Full,
      Stack_Empty => Empty
    );

    -- Turn the clock around 0..1..0..1..0...
    Clk_process: process
    begin
      -- If the clk_stop is not given, loop
      while (clk_stop = '0') loop
        Clk <= '0';
        wait for Clk_period / 2;
        Clk <= '1';
        wait for Clk_period / 2;
      end loop;
      wait;
    end process;

    -- The main process to push and pop
    stim_process: process
      begin
        -- Wait for 3 Periods
        wait for clk_period*3;
        -- Enable the stack
        Enable <= '1';
        -- Push mode
        MODE <= PUSH_MODE;


-- Push 0..1024 to stack
--        for i in 0 to STACK_HEIGHT loop
  --        Data_In <= conv_std_logic_vector(i,STACK_ELEMENT_WIDTH+1);
    --      wait for clk_period;
      --  end loop;


        -- Push 0..8 to stack
        for i in 0 to 8 loop
          Data_In <= conv_std_logic_vector(i,STACK_ELEMENT_WIDTH+1);
          wait for clk_period;
        end loop;

        -- Disable stack
        Enable <= '0'; 
        wait for clk_period*2;

        -- Reenable stack
        Enable <= '1'; 
        -- Pop mode
        MODE <= POP_MODE;

        -- Pop everything from stack
--        for i in 0 to STACK_HEIGHT loop
--          wait for clk_period;
--        end loop;
--        Enable <= '0';

--pop 8 times
  for i in 0 to 8 loop
          wait for clk_period;
        end loop;
        Enable <= '0';

        -- Push and pop random data
        wait for clk_period*3;
        Enable <= '1';
        MODE <= PUSH_MODE;
        Data_In <= X"00000001";
        wait for clk_period;
        Data_In <= X"00000002";
        wait for clk_period;        
	MODE <= INPLACE_2_MODE;
	Data_In <= X"00000003";
        wait for clk_period;
 	MODE <= INPLACE_1_MODE;
	Data_In <= X"FFFFFFFA";
        wait for clk_period;
        
        Enable <='0';
        
        -- Stop simulation
        clk_stop <= '1';        
        
        wait;
      end process;
    end;

    
