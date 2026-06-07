LAZER_ROOT ?= ../third_party/lazer
PYTHON_DIR := $(LAZER_ROOT)/python
SCRIPT_DIR := $(LAZER_ROOT)/scripts

.PHONY: all clean

all: _auth_minimal_params_cffi.o

_auth_minimal_params_cffi.o: auth_minimal_params.h $(PYTHON_DIR)/_lazer_cffi.o
	python3 $(PYTHON_DIR)/params_cffi_build.py auth_minimal_params.h $(LAZER_ROOT)

auth_minimal_params.h: auth_minimal_params.py
	cd $(SCRIPT_DIR) && sage lin-codegen.sage $(CURDIR)/auth_minimal_params.py > $(CURDIR)/auth_minimal_params.h

$(PYTHON_DIR)/_lazer_cffi.o:
	cd $(PYTHON_DIR) && make

clean:
	rm -rf __pycache__
	rm -f *.h *.c *.o *.so
