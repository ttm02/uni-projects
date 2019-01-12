-- File: stack.vhd
-- Authors: Sandro, Thomas
-- 
-- A simple LIFO stack that grows downwards. 

--INPLACE_1 und INPLACE_2 wären für eine Klassische Stack-Maschine von vortei.
-- wir Benutzen sie aber nicht.
-- INPLACE_1: Top Of Stack Inplace durch Data_In ersetzen
-- entspricht
-- pop
-- push
-- INPLACE_2: Next Of Stack Inplace durch Data_In ersetzen, dabei Top of Stack löchen (pop)
-- entspricht:
-- pop
-- pop
-- push 
--
-- Man kann also Takte für Push und Pop Sparen.

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.std_logic_arith.all; -- um den fixwert 0 in Variabler wortbreite zu erzeugen

use work.constants.all;

entity stack is
generic( width	: integer range 2 to 512;
	height : integer range 2 to 512
);
  port( Clk: in std_logic; -- Clock
        Enable: in std_logic; -- Is stack enabled?
        Data_In: in std_logic_vector((width - 1) downto 0); -- 32 bit stack input
        Top_Of_Stack: out std_logic_vector((width - 1)  downto 0);  -- 32 bit stack output
        Next_Of_Stack: out std_logic_vector((width - 1) downto 0);  -- 32 bit stack output
        MODE: in std_logic_vector (1 downto 0); -- mode
        Stack_Full: out std_logic; -- High when Stack full
        Stack_Empty: out std_logic -- High when Stack empty
        );

end stack;


architecture Behavioral of stack is

  -- Stack size: 255
  type mem_type is array (height downto 0) of std_logic_vector((width - 1) downto 0);
  signal stack_mem: mem_type := (others => (others => '0'));
  signal stack_ptr: integer:= height;
  signal full,empty: std_logic := '0';

  begin
    
    Stack_Full <= full;
    Stack_Empty <= empty;

--  enable ist nur bei rising edge wichtig, daher nicht in sensitivity list

    -- PUSH to the stack
    PUSH: process(Clk,stack_ptr,stack_mem)
    begin
      if(rising_edge(Clk)) then

        -- PUSH
        if(Enable = '1' and MODE = PUSH_MODE  and full = '0') then
          stack_mem(stack_ptr) <= Data_In;
          -- Adjust stack pointer
          if(stack_ptr /= 0) then
            stack_ptr <= stack_ptr - 1;
          end if;  
   
          end if;
        -- END PUSH
        
        -- POP
        if(Enable = '1' and MODE = POP_MODE and empty = '0') then
          -- We can pop from the stack if the SP is not at the start
          if(stack_ptr /= height) then
            -- adjust stack-pointer
            stack_ptr <= stack_ptr + 1;
		--set Data_Out
          end if;
	end if;-- end Pop

	-- INPLACE_1
        if(Enable = '1' and MODE = INPLACE_1_MODE  and full = '0') then
          stack_mem(stack_ptr + 1 ) <= Data_In;
          -- Adjust stack pointer not nessasary
       
          end if;
        -- END INPLACE_1

	-- INPLACE_2
        if(Enable = '1' and MODE = INPLACE_2_MODE  and stack_ptr < height-1  ) then-- min 2 Elemente nötig
          stack_mem(stack_ptr + 2 ) <= Data_In;
          -- Adjust stack pointer 
          if(stack_ptr /= 0) then
            stack_ptr <= stack_ptr + 1;
          end if;  
   
          end if;
        -- END INPLACE_2

      end if;
      -- END RISING_EDGE

	-- Set Full end empty
        -- If SP is 0 the stack is full
          if(stack_ptr = 0) then
            full <= '1';
            empty <= '0';
          -- If SP is height the stack is empty
          elsif(stack_ptr = height) then
            full <= '0';
            empty <= '1';
          -- Normal case
          else
            full <= '0';
            empty <= '0';
          end if;

	--adjust Tos and Next
	if(stack_ptr = height) then-- EMPTY
	Top_Of_Stack <=conv_std_logic_vector(0,width);
	Next_Of_Stack <=conv_std_logic_vector(0,width);
	elsif (stack_ptr = height-1) then --ONLY ONE ELEMENT REMAINING
	Top_Of_Stack <= stack_mem(stack_ptr + 1 );
	Next_Of_Stack <= conv_std_logic_vector(0,width);
	else
	Top_Of_Stack <= stack_mem(stack_ptr + 1);
	Next_Of_Stack <= stack_mem(stack_ptr + 2);
	end if;



      end process;      
end Behavioral;
-- END BEAVIORAL
