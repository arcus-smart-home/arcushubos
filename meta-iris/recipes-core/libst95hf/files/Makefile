#
# ST Micro ST95 NFC Library support
#

#
# Copyright 2019 Arcus Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# We build just what we need for this library...

CFLAGS	+= -Wall
CFLAGS += -DCR95HF -DIRIS
CFLAGS += -ILibraries/95HF/inc
CFLAGS += -ILibraries/NDEF/inc
CFLAGS += -IProjects/ST95HF_ETEC/inc
CFLAGS += -fPIC

.PHONY: all strip clean install uninstall

all:
	$(CC) $(CFLAGS) -c Libraries/NDEF/src/lib_NDEF.c -o lib_NDEF.o
	$(CC) $(CFLAGS) -c Libraries/NDEF/src/lib_NDEF_AAR.c -o lib_NDEF_AAR.o
	$(CC) $(CFLAGS) -c Libraries/NDEF/src/lib_NDEF_Email.c -o lib_NDEF_Email.o
	$(CC) $(CFLAGS) -c Libraries/NDEF/src/lib_NDEF_URI.c -o lib_NDEF_URI.o
	$(CC) $(CFLAGS) -c Libraries/NDEF/src/lib_95HF_wrapper.c -o lib_95HF_wrapper.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/drv_95HF.c -o drv_95HF.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/drv_spi.c -o drv_spi.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_nfctype1pcd.c -o lib_nfctype1pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_nfctype2pcd.c -o lib_nfctype2pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_nfctype3pcd.c -o lib_nfctype3pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_nfctype4pcd.c -o lib_nfctype4pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_nfctype5pcd.c -o lib_nfctype5pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_PCD.c -o lib_PCD.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_iso14443Apcd.c -o lib_iso14443Apcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_iso14443Bpcd.c -o lib_iso14443Bpcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_iso15693pcd.c -o lib_iso15693pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_iso18092pcd.c -o lib_iso18092pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_iso7816pcd.c -o lib_iso7816pcd.o
	$(CC) $(CFLAGS) -c Libraries/95HF/src/lib_ConfigManager.c -o lib_ConfigManager.o

	${CC} ${LDFLAGS} -shared -fPIC -Wl,-soname,libst95hf.so.1 -o libst95hf.so.1.0 *.o

clean:
	rm *.o
	rm *so*
