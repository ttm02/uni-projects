constants = ../constants.vhd

# Prüft, ob die Datei .ghdl im Verzeichnis vorhanden ist, wenn ja wird ghdl benutzt, ansonsten ncsim
ifneq (,$(wildcard .ghdl))
all: ghdl
else
all: ncsim
endif

ncsim:
	ncvhdl $(constants) 
	ncvhdl *.vhd
	ncelab alu_tb
	ncsim -gui alu_tb

ghdl:
	ghdl -a --workdir=work --ieee=synopsys $(constants)
	ghdl -a --workdir=work --ieee=synopsys *.vhd
	ghdl -e --workdir=work --ieee=synopsys alu_tb

# Säubert das Arbeitsverzeichnis
clean:
	echo TODO
