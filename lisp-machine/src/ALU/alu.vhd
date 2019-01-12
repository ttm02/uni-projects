-- Alu

library ieee;					-- packages:
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
--use IEEE.std_logic_arith.all;

use work.constants.all;


-- Konstanten zum Test

--  constant	opcNoOp		: std_logic_vector (29 downto 0)	:= "000000000000000000000000000000";
--  constant	opcNegA		: std_logic_vector (29 downto 0)	:= "000000000000000000000000000001";



-- Daten bestehen immer aus 30 Bit, wegen 2 Bit Tag im Speicher

--entity (Struktur)
entity ALU is
port(	Data1	: in	std_logic_vector(29 downto 0);	-- Data 1
	Data2	: in	std_logic_vector(29 downto 0);	-- Data 2
	Data3	: in	std_logic_vector(29 downto 0);	-- Data 3
	Data4	: in	std_logic_vector(29 downto 0);	-- Data 4
	Tag1	: in	std_logic_vector(1 downto 0);	-- Tag 1
	Tag2	: in	std_logic_vector(1 downto 0);	-- Tag 2
	Tag3	: in	std_logic_vector(1 downto 0);	-- Tag 3
	Tag4	: in	std_logic_vector(1 downto 0);	-- Tag 4

	Data_OUT	: out	std_logic_vector(29 downto 0);	-- Datenausgang
	Tag_OUT		: out	std_logic_vector(1 downto 0) );	-- Ausgang für das Tag

end entity ALU;


-- architecture (Verhalten)

architecture rechnen of ALU is

signal temp : std_logic_vector(89 downto 0);
signal temp2 : std_logic_vector(59 downto 0);

begin

	P1: process (Data1, Data2, Data3, Data4, Tag1, Tag2, Tag3, Tag4) is
		begin

	case Data1 is 
	when opcNoOp =>
		--NoOp
		--Tag_OUT <= TagPointer;
-- Bugfix: Tag Durchreichen, nicht "Blind" auf Pointer setzen
		Tag_OUT <= Tag2;
		Data_OUT <= Data2;

--NOP Function CALL
	when opcEvalFunc =>
		--NoOp
		Tag_OUT <= Tag2;
		Data_OUT <= Data2;

	when opcIf =>
		--if
		--Tag_OUT <= TagPointer;
		if (Tag2 = TagData)  then  --and (Tag3 = TagPointer) and (Tag4 = TagPointer)
			if (Data2 > NullVector) then
				Tag_Out <= Tag3;
				Data_OUT <= Data3;
			else
				Tag_Out <= Tag4;
				Data_OUT <= Data4;
			end if;
		else
		Tag_OUT <= TagError;
		Data_OUT <= ErrorIf;
		end if;
-- Tags Durchreichen nicht "blind" auf Pointer setzen

	when opcADD =>
		--ADD
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			Data_OUT <= std_logic_vector(signed(Data2)+signed(Data3)+signed(Data4));
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			Data_OUT <= std_logic_vector(signed(Data2)+signed(Data3));
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcSUB =>
		--SUB
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			Data_OUT <= std_logic_vector(signed(Data2)-signed(Data3)-signed(Data4));
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			Data_OUT <= std_logic_vector(signed(Data2)-signed(Data3));
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcMUL =>
		--MUL
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			temp <= std_logic_vector(signed(Data2)*signed(Data3)*signed(Data4));
			Data_OUT <= temp(29 downto 0);
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			temp2 <= std_logic_vector(signed(Data2)*signed(Data3));
			Data_OUT <= temp(29 downto 0);
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcDIV =>
		--DIV
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			Data_OUT <= std_logic_vector(signed(Data2)/signed(Data3)/signed(Data4));
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			Data_OUT <= std_logic_vector(signed(Data2)/signed(Data3));
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcNeg =>
		--Negiere Data2
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= not (Data2);
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcAND =>
		--Boolsches AND der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			if((Data2 = FalseVector) or (Data3 = FalseVector) or (Data4 = FalseVector)) then
				Data_OUT <= FalseVector;
			else
				Data_OUT <= TrueVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if((Data2 = FalseVector) or (Data3 = FalseVector)) then
				Data_OUT <= FalseVector;
			else
				Data_OUT <= TrueVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcOR =>
		--Boolsches OR der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			if((Data2 = FalseVector) and (Data3 = FalseVector) and (Data4 = FalseVector)) then
				Data_OUT <= FalseVector;
			else
				Data_OUT <= TrueVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if((Data2 = FalseVector) and (Data3 = FalseVector)) then
				Data_OUT <= FalseVector;
			else
				Data_OUT <= TrueVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcXOR =>
		--Boolsches XOR der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			if(((Data2 = FalseVector) and (Data3 = FalseVector) and (Data4 = FalseVector)) or ((Data2 = TrueVector) and (Data3 = TrueVector) and (Data4 = TrueVector)) ) then
				Data_OUT <= FalseVector;
			else
				Data_OUT <= TrueVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if(((Data2 = FalseVector) and (Data3 = FalseVector)) or ((Data2 = TrueVector) and (Data3 = TrueVector)) ) then
				Data_OUT <= FalseVector;
			else
				Data_OUT <= TrueVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcNAND =>
		--Boolsches NAND der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagData) then
			if(((Data2 = FalseVector) and (Data3 = FalseVector) and (Data4 = FalseVector))) then
				Data_OUT <= TrueVector;
			else
				Data_OUT <= FalseVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if((Data2 = FalseVector) and (Data3 = FalseVector)) then
				Data_OUT <= TrueVector;
			else
				Data_OUT <= FalseVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when opcEqual =>
		--Boolsches Equal der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if(Data2 = Data3) then
				Data_OUT <= TrueVector;
			else
				Data_OUT <= FalseVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= Error_2_Data_req;
		end if;

	when opcNotEqual =>
		--Boolsches NotEqual der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if(Data2 /= Data3) then
				Data_OUT <= TrueVector;
			else
				Data_OUT <= FalseVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= Error_2_Data_req;
		end if;

	when opcLessThan =>
		--Boolsches LessThan der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if(Data2 < Data3) then
				Data_OUT <= TrueVector;
			else
				Data_OUT <= FalseVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= Error_2_Data_req;
		end if;

	when opcGreaterThan =>
		--Boolsches GreaterThan der Daten
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			if(Data2 > Data3) then
				Data_OUT <= TrueVector;
			else
				Data_OUT <= FalseVector;
			end if;
		elsif (Tag2 = TagData) and (Tag3 = TagNullList) and (Tag4 = TagNullList) then
			Data_OUT <= Data2;
		else
			Tag_OUT <= TagError;
			Data_OUT <= Error_2_Data_req;
		end if;

	when opcLogicShiftR =>
		--Logisher Shift der Daten. Data2 = Data to be shifted; Data3 = number of bits
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			Data_OUT <= std_logic_vector(unsigned(Data2) srl to_integer(unsigned(Data3)));
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when ShiftR =>
		--Arithmetischer Shift der Daten. Data2 = Data to be shifted; Data3 = number of bits
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			Data_OUT <= std_logic_vector(unsigned(Data2) sra to_integer(unsigned(Data3)));
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;

	when ShiftL =>
		--Arithmetischer Shift der Daten. Data2 = Data to be shifted; Data3 = number of bits
		Tag_OUT <= TagData;
		if (Tag2 = TagData) and (Tag3 = TagData) and (Tag4 = TagNullList) then
			Data_OUT <= std_logic_vector(unsigned(Data2) sla to_integer(unsigned(Data3)));
		else
			Tag_OUT <= TagError;
			Data_OUT <= ErrorDataMissing;
		end if;	


	when others =>



		end case;

	end process P1;

end architecture rechnen ;


