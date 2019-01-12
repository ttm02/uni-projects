-- controller
-- Fetch and decode an instruction

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.std_logic_arith.all;

USE work.constants.all;

use work.sramPkg.all;



-- Use ALU
-- USE STACK
-- USE SRAM

entity controller is
--generic(
--MEMFILE	: string	:= "sram.dat"); -- filename for memoryfile
-- ist generic fom Speicher, der wird erst in der Testbench instanziiert

port(
clk: in std_logic ;
--fileIO : in     fileIOty       := none;

Memaddr: out std_logic_vector (31 downto 0);
MemWrite: out std_logic_vector (127 downto 0);

MemRead: in std_logic_vector (127 downto 0);

MemWenable: out std_logic

); --clk sim:/proctst/instControll/Nxt_PC



--signal PC : std_logic_vector(29 downto 0) := (others => '0'); -- Programm counter -- not needed
signal Nxt_Inst : std_logic_vector(127 downto 0) := (others => '0'); -- next loaded instruction

signal NXt_Pc : std_logic_vector(29 downto 0) := (others => '0'); -- adress for next load


signal Data1 : std_logic_vector(29 downto 0) := (others => '0'); -- Data for Alu
signal Data2 : std_logic_vector(29 downto 0) := (others => '0'); -- 
signal Data3 : std_logic_vector(29 downto 0) := (others => '0'); -- 
signal Data4 : std_logic_vector(29 downto 0) := (others => '0'); -- 


signal Tag1 : std_logic_vector(1 downto 0) := (others => '0'); -- Tags for Alu
signal Tag2 : std_logic_vector(1 downto 0) := (others => '0'); -- Tags for Alu
signal Tag3 : std_logic_vector(1 downto 0) := (others => '0'); -- Tags for Alu
signal Tag4 : std_logic_vector(1 downto 0) := (others => '0'); -- Tags for Alu


signal Data_Out : std_logic_vector(29 downto 0) := (others => '0'); -- Output from Alu
signal Tag_out : std_logic_vector(1 downto 0) := (others => '0'); -- 


-- Stack for Function Parameter
-- FStack
signal FStack_In : std_logic_vector(127 downto 0) := (others => '0'); -- Input to Stack
signal Top_Of_FStack: std_logic_vector(127 downto 0) := (others => '0');  -- 32 bit stack output
signal Next_Of_FStack: std_logic_vector(127  downto 0) := (others => '0');  -- 32 bit stack output


signal FStack_Enable: std_logic := '0';
signal FStack_Mode:std_logic_vector (1 downto 0) := (others => '0'); -- mode
signal FStack_Full: std_logic := '0'; -- High when Stack full
signal FStack_Empty: std_logic := '0';-- High when Stack empty


-- Call Stack
signal Stack_In : std_logic_vector(129 downto 0) := (others => '0'); -- Input to Stack
signal Top_Of_Stack: std_logic_vector(129 downto 0) := (others => '0');  -- 32 bit stack output
signal Next_Of_Stack: std_logic_vector(129  downto 0) := (others => '0');  -- 32 bit stack output


signal Stack_Enable: std_logic := '0';
signal Stack_Mode:std_logic_vector (1 downto 0) := (others => '0'); -- mode
signal Stack_Full: std_logic := '0'; -- High when Stack full
signal Stack_Empty: std_logic := '0';-- High when Stack empty


signal load:  std_logic := '0';-- load next inst from mem or from stack


signal nWE: std_logic := '1'; --Mnot Write enabled
signal Raddr:std_logic_vector (31 downto 0) := (others => '0'); -- Address for mem
signal Waddr:std_logic_vector (1 downto 0) := (others => '0'); -- 

signal DIn : std_logic_vector(31 downto 0) := (others => '0'); -- Result Input

signal Page:std_logic_vector (127 downto 0) := (others => '0'); -- Contains the loaded Page = Fetched Instruction

signal RESULT :std_logic_vector (29 downto 0) := (others => '0'); -- Signal with Program Result --u may use it for for Output 
-- Result wird mit allen Berechnungsergebnissen gefüllt bleibt dann am ende Konstant auf dem Endergebnis

signal Termination : std_logic := '1'; -- 1 = nicht Terminieren
-- Bei Termination hat RESULT das Abschließende Ergebnis

signal const0 : std_logic := '0';
signal const1 : std_logic := '1';

signal nCS : std_logic := '0';




end entity controller;


architecture Control of controller is
begin

-- Instanziiere "ALU"
    alu: entity work.ALU port map (
      	Data1 => Data1,
	Data2 => Data2,
	Data3 => Data3,
	Data4 => Data4,
	Tag1 => Tag1,
	Tag2 => Tag2,
	Tag3 => Tag3,
	Tag4 => Tag4,
	
	Data_Out => Data_Out,
	Tag_OUT => Tag_out
    );


-- instanziiere call Stack
 stack: entity work.Stack 
generic map(width =>130,
		height=>128)
port map (
      	clk => clk,
	enable => Stack_Enable,
	Data_IN => Stack_In,
	Top_Of_Stack => Top_Of_Stack,
	Next_Of_Stack => Next_Of_Stack,
        MODE => Stack_Mode,
        Stack_Full => Stack_Full,
        Stack_Empty => Stack_Empty
        );

-- instanziiere Function FStack
 Fstack: entity work.Stack 
generic map(width =>128,
		height=>128)
port map (
      	clk => clk,
	enable => FStack_Enable,
	Data_IN => FStack_In,
	Top_Of_Stack => Top_Of_FStack,
	Next_Of_Stack => Next_Of_FStack,
        MODE => FStack_Mode,
        Stack_Full => FStack_Full,
        Stack_Empty => FStack_Empty
        );
 
--instanziiere instruction Fetch Register
-- erste instruktion ist dann initialisiert
-- erste instruktion= werte das Programm aus
inst: entity work.regIni 
-- generic map (width => 128)
port map (
clk =>clk,
din => Nxt_Inst,
dout => Page
);

-- instanziiere Result Register
ResREG: entity work.reg
 generic map (width => 30)
port map (
clk =>clk,
enable => Termination, -- Bei Termination wird das Register einfach abgeschaltet
din => Data_Out,
dout => Result
);
-- die Maschine Terminiert also nach außen hin
-- nach innen wird die Letzte Instruktion (die dann eval Ergebnis ist) immer wieder ausgewertet


--instanziiere Ram
-- Ram wird nun cin Außen an den controler angelegt
--MEM: entity work.sram	generic map (	addrWd	=> 8,
--					dataWd	=> 32,
--					fileID	=> MEMFILE)
--			port map    (	clk =>clk,
--					nCS	=> nCS,
--					nWE	=> nWe,
--					Read_addr	=> Raddr(7 downto 0),
--					dataIn	=> DIn,
--					Write_addr	=> WAddr(7 downto 0),
--					dataOut	=> Page,
--
--					fileIO	=> fileIO);
--

split: process(Page,Top_Of_FStack)
begin
--Aufsplitten der Geladenen Seite =
-- alle Daten Entsprechend Anlegen

-- Ausserdem: Funktionsparameter von entsprechendem Stack Laden falls nötig
	if (Page(127 downto 96)= "11000000000000000000000000000001") then -- Parameter 1 Laden
	tag1 <= Top_Of_FStack(95 downto 94);
	Data1 <= Top_Of_FStack(93 downto 64);
	elsif (Page(127 downto 96)= "11000000000000000000000000000010") then -- Parameter 2 Laden
	tag1 <= Top_Of_FStack(63 downto 62);
	Data1 <= Top_Of_FStack(61 downto 32);
	elsif (Page(127 downto 96)= "11000000000000000000000000000011") then -- Parameter 3 Laden
	tag1 <= Top_Of_FStack(31 downto 30);
	Data1 <= Top_Of_FStack(29 downto 0);
	else -- normale Daten
	tag1 <= Page(127 downto 126);
	Data1 <= Page(125 downto 96);
	end if;
-- bei  erster stelle sollten immer normale Daten kommen
-- wenn man aber z.B. den OpCode als Parameter übergibt
-- Ist es auch erlaubt

if (Page(95 downto 64)= "11000000000000000000000000000001") then -- Parameter 1 Laden
	tag2 <= Top_Of_FStack(95 downto 94);
	Data2 <= Top_Of_FStack(93 downto 64);
	elsif (Page(95 downto 64)= "11000000000000000000000000000010") then -- Parameter 2 Laden
	tag2 <= Top_Of_FStack(63 downto 62);
	Data2 <= Top_Of_FStack(61 downto 32);
	elsif (Page(95 downto 64)= "11000000000000000000000000000011") then -- Parameter 3 Laden
	tag2 <= Top_Of_FStack(31 downto 30);
	Data2 <= Top_Of_FStack(29 downto 0);
	else -- normale Daten
	Tag2 <= Page(95 downto 94);
	Data2 <= Page(93 downto 64);
	end if;

if (Page(63 downto 32)= "11000000000000000000000000000001") then -- Parameter 1 Laden
	tag3 <= Top_Of_FStack(95 downto 94);
	Data3 <= Top_Of_FStack(93 downto 64);
	elsif (Page(63 downto 32)= "11000000000000000000000000000010") then -- Parameter 2 Laden
	tag3 <= Top_Of_FStack(63 downto 62);
	Data3 <= Top_Of_FStack(61 downto 32);
	elsif (Page(63 downto 32)= "11000000000000000000000000000011") then -- Parameter 3 Laden
	tag3 <= Top_Of_FStack(31 downto 30);
	Data3 <= Top_Of_FStack(29 downto 0);
	else -- normale Daten
	Tag3 <= Page(63 downto 62);
	Data3 <= Page(61 downto 32);
	end if;

if (Page(32 downto 0)= "11000000000000000000000000000001") then -- Parameter 1 Laden
	tag4 <= Top_Of_FStack(95 downto 94);
	Data4 <= Top_Of_FStack(93 downto 64);
	elsif (Page(32 downto 0)= "11000000000000000000000000000010") then -- Parameter 2 Laden
	tag4 <= Top_Of_FStack(63 downto 62);
	Data4 <= Top_Of_FStack(61 downto 32);
	elsif (Page(32 downto 0)= "11000000000000000000000000000011") then -- Parameter 3 Laden
	tag4 <= Top_Of_FStack(31 downto 30);
	Data4 <= Top_Of_FStack(29 downto 0);
	else -- normale Daten
	tag4 <= Page(31 downto 30);
	Data4 <= Page(29 downto 0);
	end if;
	
end process;



-- Prozess der Tag und Datenausgang der ALU wieder zusammenführt
ResultUpdate: process(Data_Out,Tag_Out)
begin
DIn <= Tag_Out & Data_Out; 
end process;

-- LEGACY
--ERsetzt durch fetchNxt
-- Prozess, der den Mem ansteuert, dabei werden die Signale nur entsprechend gemappt, damit sie im übrigen code nicht ersetzt werden müssen
--mem: process(Clk,Waddr,Raddr,MemRead,nwe)
--begin
--Memaddr <=Raddr;
-- Page <= MemRead;

	-- Schreiben 
--	 MemWenable <= not nwe;
--		if ( Waddr(1 downto 0) = "00") then
--			MemWrite <= Tag_Out & Data_Out & Page(95 downto 0);
--		elsif (Waddr(1 downto 0) = "01") then
--			MemWrite <= Page(127 downto 96) & Tag_Out & Data_Out & Page(63 downto 0);
--		elsif (Waddr(1 downto 0) = "10") then
--			MemWrite <= Page(127 downto 64) & Tag_Out & Data_Out & Page(31 downto 0);
--		else
--			MemWrite <= Page(127 downto 32) & Tag_Out & Data_Out;
--		end if;
		

--end process;


-- Prozess, der die Nächste anweisung lädt
fetchNxt: process(load,Waddr,NXt_PC,MemRead,Top_Of_Stack,Tag_Out,Data_Out)
begin


	if (load = '1') then-- load from mem
	Memaddr <= "00" & NXt_PC;-- 32 Bit Adresse erzeugen
	Nxt_Inst <= MemRead;

	else -- get from stack
--	 Von Stack
	 MemWenable <= not nwe;
		if ( Waddr = "00") then
			Nxt_Inst <= Tag_Out & Data_Out & Top_Of_Stack(95 downto 0);
		elsif (Waddr = "01") then
			Nxt_Inst <= Top_Of_Stack(127 downto 96) & Tag_Out & Data_Out & Top_Of_Stack(63 downto 0);
		elsif (Waddr = "10") then
			Nxt_Inst <= Top_Of_Stack(127 downto 64) & Tag_Out & Data_Out & Top_Of_Stack(31 downto 0);
		else
			Nxt_Inst <= Top_Of_Stack(127 downto 32) & Tag_Out & Data_Out;
		end if;
--Beim Laden Vom Stack muss ein Teil durch das Gerade Berechnete Ergebnis ersetzt werden

	 end if; -- load = 

end process;


-- Hauptteil
-- Stelt Fest in Welchem Fall man ist, und wohin der Kontrollfluss weitergehen soll
decode: process(Data1,Data2,Data3,Data4,Tag1,Tag2,Tag3,Tag4)
begin

	if (Data1 = opcIf) then -- special expression

		if (Tag2 = TagPointer ) then -- Bedingung muss noch ausgewertet werden
			-- CALL

			nwe <= '1';
			load <= '1';

			NXt_PC <=Data2;

			Stack_Mode <= PUSH_MODE;
			Stack_In <= "01" & Page;-- Return Addr + Result Offset
	
			Stack_Enable <= '1';

		else

		if Stack_Empty = '1' then 
			--Termination
			--Result <= Tag_Out & Data_Out;
			Termination <= '0';
			
			else
			--WRITEBACK!			
			load <= '0';
			Waddr <= Top_Of_Stack(129 downto 128);--Result addr = top of Stack
			Stack_Mode <= POP_MODE;
			Stack_Enable <= '1';			
			
			
		end if;

		end if;

	else --no special expression;

		if (Tag2 = TagPointer ) then
		-- CALL2
		nwe <= '1';
		load <= '1';
		NXt_PC <=Data2;
		Stack_Mode <= PUSH_MODE;
		Stack_In <= "01" & Page;-- Return Addr + Result Offset -- & = Konkatenation
		Stack_Enable <= '1';
		FStack_Enable <= '0';
	
		elsif (Tag3 = TagPointer ) then
		-- CALL3
		nwe <= '1';
		load <= '1';
		NXt_PC <=Data3;
		Stack_Mode <= PUSH_MODE;
		Stack_In <= "10" & Page;-- Return Addr + Result Offset
		Stack_Enable <= '1';
		FStack_Enable <= '0';

		elsif (Tag4 = TagPointer ) then
		-- CALL4
		nwe <= '1';
		load <= '1';
		NXt_PC <=Data4;
		Stack_Mode <= PUSH_MODE;
		Stack_In <= "11" & Page;-- Return Addr + Result Offset
		Stack_Enable <= '1';
		FStack_Enable <= '0';
		
		elsif (Data1(29 downto 29) = "1") then
		-- Function Call
		nwe <= '1';
		load <= '1';
		NXt_PC <= "0" & Data1(28 downto 0); -- erstes Bit Zeigt an, dass es sich um Funktion handelt und ist nicht teil der Adresse
		FStack_Mode <= PUSH_MODE;
		FStack_In <= Page;-- Parameter-Stack
		FStack_Enable <= '1';
		Stack_Enable <= '0';
		

		

		else 
			if Stack_Empty = '1' then
			--Termination
			--Result <= Tag_Out & Data_Out;
			Termination <= '0';
			
			else

			if (Data1 = opcEvalFunc) then 
			-- Funktionsrücksprung
			FStack_Enable <= '1';	
			FStack_Mode <= POP_MODE;
			else
			FStack_Enable <= '0';
			end if;

			load <= '0';
			Waddr <= Top_Of_Stack(129 downto 128);--Result addr = top of Stack
			Stack_Mode <= POP_MODE;
			Stack_Enable <= '1';

			end if;

		end if;

end if;


end process;



end control;

