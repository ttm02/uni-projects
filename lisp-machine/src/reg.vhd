-- reg.vhd					AJM : 25.11.2002
--

-- angepasst: enable Signal hinzugefügt
-- entity	reg	-parametrisable register
-- architecture	behavior
------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity reg is
generic( width	: integer range 2 to 512);
port	 ( clk		: in std_logic;
	enable : in std_Logic;
	   dIn		: in  std_logic_vector(width-1 downto 0);
	   dOut		: out std_logic_vector(width-1 downto 0) := (others => '0'));
end entity reg;

architecture behavior of reg is
begin
  reg_P: process (clk) is
  begin
    if (rising_edge(clk) and (enable='1')) then dOut <= dIn;
    end if;
  end process reg_P;
end architecture behavior;
