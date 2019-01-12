# Common definitions
CC = gcc `pkg-config --cflags --libs glib-2.0`
mpi = mpicc `pkg-config --cflags --libs glib-2.0`

# Compiler flags, paths and libraries
#CFLAGS = -std=c11 -pedantic -Wall -Wextra -ggdb
#CFLAGS = -std=c11 -fopenmp -pedantic -Wall -Wextra -ggdb 
CFLAGS = -std=c11 -fopenmp -pedantic -Wall -Wextra -O3
CFLAGSMPI = -D USE_MPI $(CFLAGS)


LFLAGS = $(CFLAGS)
LIBS   = -lm

TGTS = measure configure mpi-wrapper
OBJS = metric1.o CPUmetrics.o cache.o diskmetrics.o netmetrics.o metric_definitions.o
OBJSAN = omp_kmeans.o

# Targets ...
all: $(TGTS)

measure: measure.o $(OBJS) Makefile
	$(CC) $(LFLAGS) -o $@ measure.o $(OBJS) $(LIBS)

measure.o: measure.c Makefile

configure: configure.o Makefile
	$(CC) $(LFLAGS) -o $@ configure.o $(LIBS)
	
configure.o: configure.c Makefile

analyze: analyze.o $(OBJSAN) $(OBJS) Makefile
	$(CC) $(LFLAGS) -o $@ analyze.o $(OBJSAN) $(OBJS) $(LIBS)
	
analyze.o: analyze.c Makefile

# compile measure.c using mpi compiler and defined USE_MPI
mpi-wrapper: mpi-wrapper.o $(OBJS) Makefile
	$(mpi) $(CFLAGSMPI) -o $@ mpi-wrapper.o $(OBJS) $(LIBS)

mpi-wrapper.o: measure.c Makefile
	$(mpi) $(CFLAGSMPI) -o mpi-wrapper.o -c measure.c

# Rule to create *.o from *.c
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c

clean:
	$(RM) measure.o configure.o analyze.o mpi-wrapper.o
	$(RM) $(OBJS)
	$(RM) $(OBJSAN)
	$(RM) $(TGTS)
