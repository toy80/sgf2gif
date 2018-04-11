
# for GNU Make + MinGW 

H_3RD := 3rdpart/CxImage/xfile.h \
 3rdpart/CxImage/ximabmp.h \
 3rdpart/CxImage/ximacfg.h \
 3rdpart/CxImage/ximadef.h \
 3rdpart/CxImage/ximage.h \
 3rdpart/CxImage/ximagif.h \
 3rdpart/CxImage/ximaico.h \
 3rdpart/CxImage/ximaiter.h \
 3rdpart/CxImage/ximajas.h \
 3rdpart/CxImage/ximajbg.h \
 3rdpart/CxImage/ximajpg.h \
 3rdpart/CxImage/ximamng.h \
 3rdpart/CxImage/ximapcx.h \
 3rdpart/CxImage/ximapng.h \
 3rdpart/CxImage/ximapsd.h \
 3rdpart/CxImage/ximaraw.h \
 3rdpart/CxImage/ximaska.h \
 3rdpart/CxImage/ximatga.h \
 3rdpart/CxImage/ximath.h \
 3rdpart/CxImage/ximatif.h \
 3rdpart/CxImage/ximawbmp.h \
 3rdpart/CxImage/ximawmf.h \
 3rdpart/CxImage/xiofile.h \
 3rdpart/CxImage/xmemfile.h \
 3rdpart/simplexnoise1234.h \
 3rdpart/Quantize.h

C_3RD := 3rdpart/CxImage/tif_xfile.cpp \
 3rdpart/CxImage/ximabmp.cpp \
 3rdpart/CxImage/ximadsp.cpp \
 3rdpart/CxImage/ximaenc.cpp \
 3rdpart/CxImage/ximaexif.cpp \
 3rdpart/CxImage/ximage.cpp \
 3rdpart/CxImage/ximagif.cpp \
 3rdpart/CxImage/ximahist.cpp \
 3rdpart/CxImage/ximaico.cpp \
 3rdpart/CxImage/ximainfo.cpp \
 3rdpart/CxImage/ximaint.cpp \
 3rdpart/CxImage/ximajas.cpp \
 3rdpart/CxImage/ximajbg.cpp \
 3rdpart/CxImage/ximajpg.cpp \
 3rdpart/CxImage/ximalpha.cpp \
 3rdpart/CxImage/ximalyr.cpp \
 3rdpart/CxImage/ximamng.cpp \
 3rdpart/CxImage/ximapal.cpp \
 3rdpart/CxImage/ximapcx.cpp \
 3rdpart/CxImage/ximapng.cpp \
 3rdpart/CxImage/ximapsd.cpp \
 3rdpart/CxImage/ximaraw.cpp \
 3rdpart/CxImage/ximasel.cpp \
 3rdpart/CxImage/ximaska.cpp \
 3rdpart/CxImage/ximatga.cpp \
 3rdpart/CxImage/ximath.cpp \
 3rdpart/CxImage/ximatif.cpp \
 3rdpart/CxImage/ximatran.cpp \
 3rdpart/CxImage/ximawbmp.cpp \
 3rdpart/CxImage/ximawmf.cpp \
 3rdpart/CxImage/ximawnd.cpp \
 3rdpart/CxImage/xmemfile.cpp \
 3rdpart/simplexnoise1234.cpp \
 3rdpart/Quantize.cpp

O_3RD := tif_xfile.o \
 ximabmp.o \
 ximadsp.o \
 ximaenc.o \
 ximaexif.o \
 ximage.o \
 ximagif.o \
 ximahist.o \
 ximaico.o \
 ximainfo.o \
 ximaint.o \
 ximajas.o \
 ximajbg.o \
 ximajpg.o \
 ximalpha.o \
 ximalyr.o \
 ximamng.o \
 ximapal.o \
 ximapcx.o \
 ximapng.o \
 ximapsd.o \
 ximaraw.o \
 ximasel.o \
 ximaska.o \
 ximatga.o \
 ximath.o \
 ximatif.o \
 ximatran.o \
 ximawbmp.o \
 ximawmf.o \
 ximawnd.o \
 xmemfile.o \
 simplexnoise1234.o \
 Quantize.o


.PHONY: all
all: bin/sgf2gif_eng.exe 

bin/sgf2gif_eng.exe: sgf2gif.o sgf2gif.en.res $(O_3RD) 
	g++ -o bin/sgf2gif_eng.exe sgf2gif.o sgf2gif.en.res $(O_3RD) -lgdiplus -lgdi32 -lcomctl32 -lcomdlg32 -lshlwapi

sgf2gif.o: sgf2gif.cpp res/resource.h $(H_3RD)
	g++ -D S2F_ENG -c sgf2gif.cpp


sgf2gif.en.res: res/resource.h res/sgf2gif.ico res/sgf2gif.en.rc res/xpstyle.manifest
	windres -D EXCLUDE_XPSTYLE -O coff -i res/sgf2gif.en.rc -o sgf2gif.en.res

$(O_3RD): $(C_3RD) $(H_3RD)
	g++ -c $(C_3RD)

.PHONY: clean 
clean:
	del /Q *.o
	del /Q *.res 
	del /Q bin\sgf2gif_eng.exe 
