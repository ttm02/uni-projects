-- reg.vhd					AJM : 25.11.2002
--
-- entity	regIni	-parametrisable register
-- architecture	behavior

-- register ist mit erster anweisung für die Maschine initialisiert
-- ertste Anweisung: werte das Programm aus
-- hat kein  enable, da immer eingeschaltet
------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity regIni is
--generic( width	: integer range 2 to 512);
port	 ( clk		: in std_logic;
	   dIn		: in  std_logic_vector(127 downto 0);
	   dOut		: out std_logic_vector(127 downto 0) := "01000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
end entity regIni;

architecture behavior of regIni is
begin
  reg_P: process (clk) is
  begin
    if rising_edge(clk) then dOut <= dIn;
    end if;
  end process reg_P;
end architecture behavior;
