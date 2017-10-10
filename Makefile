CC = mpicc
CXX = mpicxx
CFLAGS = -O3 -std=gnu99
CXXFLAGS = -O3 -std=gnu++11

STUDENTID = $(USER:p%=%)
EXE1 = HW1_$(STUDENTID)_basic
EXE2 = HW1_$(STUDENTID)_advanced

.PHONY: all
all: $(EXE1).cc $(EXE2).cc
	$(CXX) $(EXE1).cc $(CXXFLAGS) -o $(EXE1)
	$(CXX) $(EXE2).cc $(CXXFLAGS) -o $(EXE2)
  
.PHONY: clean
clean: 
	rm -f $(EXE1) $(EXE2)
