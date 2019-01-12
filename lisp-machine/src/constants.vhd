-- File: constants.vhdl
--
--

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.std_logic_arith.all;

package constants is

  constant      STACK_HEIGHT:                   integer         := 127;
  constant      STACK_ELEMENT_WIDTH:            integer         := 31;

-- Stack Konstanten
  constant      POP_MODE:                       std_logic_vector (1 downto 0)   := "00";
  constant      PUSH_MODE:                      std_logic_vector (1 downto 0)   := "01";
  constant 	INPLACE_1_MODE:			std_logic_vector (1 downto 0)   := "10";
  constant 	INPLACE_2_MODE:			std_logic_vector (1 downto 0)   := "11";
-- INPLACE_1: Top Of Stack Inplace durch Data_In ersetzen
-- entspricht
-- pop
-- push
-- INPLACE_2: Next Of Stack Inplace durch Data_In ersetzen, dabei Top of Stack löchen (pop)
-- entspricht:
-- pop
-- pop
-- push 

-- Tag Constants
  constant 	TagNullList			: std_logic_vector (1 downto 0)   := "00";
  constant 	TagData				: std_logic_vector (1 downto 0)   := "01";
  constant 	TagPointer			: std_logic_vector (1 downto 0)   := "10";
  constant 	TagError			: std_logic_vector (1 downto 0)   := "11";

-- TagError Data_OUT Constants
  constant 	ErrorDataMissing	: std_logic_vector (29 downto 0)	:= "000000000000000000000000001111";
  constant 	ErrorUndefinedOPC	: std_logic_vector (29 downto 0)	:= "000000000000000000000000011111";
  constant 	ErrorIf			: std_logic_vector (29 downto 0)	:= "000000000000000000000000101111";
  constant 	Error_2_Data_req	: std_logic_vector (29 downto 0)	:= "000000000000000000000000111111";

-- Error-werte immer mit den Letzten 4 Bits gesetzt um Anzueigen, dass es wirklich Error sind
-- Ein Datenwort mit einem Error-Tag, bei dem die Letzten 4 Bits nicht gesetzt sind, Ist ein Besonders Reserviertes wort (s.u.)

-- Referenz auf Funktions Parameter
-- Trifft man auf diese Referenz, wird sie vor der Auswertung durch den Entsprechenden Parameter ersetzt.
-- Es werden als Platzhalter die Error- Werte 1-3 genommen
-- wobei die Letzten 4 Bits nicht gesetzt werden, um anzuzeigen, dass dies besondere Werte sind
constant 	X1	: std_logic_vector (31 downto 0)	:="11000000000000000000000000000001"
constant 	X2	: std_logic_vector (31 downto 0)	:="11000000000000000000000000000010"
constant 	X3	: std_logic_vector (31 downto 0)	:="11000000000000000000000000000011"

-- ALU 30Bit 0 vector
  constant	NullVector			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000000";

-- ALU 30Bit Boolean vector
  constant	FalseVector			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000000";
  constant 	TrueVector			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000001";

-- ALU OPCodes

-- noOp entspricht eval
  constant	opcNoOp			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000000";
  constant	opcEval			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000000";

  constant	opcIf			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000001";

  constant	opcNeg			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000010";
  constant	opcADD			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000011";
  constant	opcSUB			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000100";
  constant	opcMUL			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000101";
  constant	opcDIV			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000110";
  constant	opcAND			: std_logic_vector (29 downto 0)	:= "000000000000000000000000000111";
  constant	opcOR			: std_logic_vector (29 downto 0)	:= "000000000000000000000000001000";
  constant	opcXOR			: std_logic_vector (29 downto 0)	:= "000000000000000000000000001001";
  constant	opcNAND			: std_logic_vector (29 downto 0)	:= "000000000000000000000000001010";
  constant	opcShiftR		: std_logic_vector (29 downto 0)	:= "000000000000000000000000001011";
  constant	opcShiftL		: std_logic_vector (29 downto 0)	:= "000000000000000000000000001100";
  constant	opcRotateR		: std_logic_vector (29 downto 0)	:= "000000000000000000000000001101";
  constant	opcRotateL		: std_logic_vector (29 downto 0)	:= "000000000000000000000000001110";
  constant	opcEqual		: std_logic_vector (29 downto 0)	:= "000000000000000000000000001111";
  constant	opcNotEqual		: std_logic_vector (29 downto 0)	:= "000000000000000000000000010001";
  constant	opcLessThan		: std_logic_vector (29 downto 0)	:= "000000000000000000000000010010";
  constant	opcGreaterThan		: std_logic_vector (29 downto 0)	:= "000000000000000000000000010011";
  constant	opcLogicShiftL		: std_logic_vector (29 downto 0)	:= "000000000000000000000000010100";
  constant	opcLogicShiftR		: std_logic_vector (29 downto 0)	:= "000000000000000000000000010101";

  constant	opcEvalFunc		: std_logic_vector (29 downto 0)	:= "011111111111111111111111111111";

end constants;
