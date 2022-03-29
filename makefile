CC = gcc
LD = gcc

HPATH   	=	-iquote ../../ds/single_threaded_scheduler/ \
				-iquote ../../ds/sorted_llist/ \
				-iquote ../../ds/dl_list/ \
				-iquote ../../ds/uid/ \
				-iquote ../../system_programming/semaphore

VPATH   	=	../../ds/single_threaded_scheduler/: \
				../../ds/sorted_llist/: \
				../../ds/dl_list/: \
				../../ds/vector/: \
				../../ds/uid: \
				../../system_programming/semaphore/:
			  
OBJECTS 	=	wd.o wd_app.o st_scheduler.o task.o uid.o \
		   		process_semaphore.o sorted_dl_list.o dl_list.o

CFLAGS  	=	-ansi -pedantic-errors -Wall -Wextra -g -pthread -fPIC $(HPATH)
LDFLAGS     =	-shared 

EXECUTABLE 	=	test
DAEMON 		=	wd_bg_p
SO 			=	wd.so

all:  $(EXECUTABLE) $(DAEMON)

$(SO): $(OBJECTS)
	$(CC) $(LDFLAGS) -g -o $@ $^

$(DAEMON): $(DAEMON).o $(SO)
	$(CC) $(CFLAGS) -Wl,-rpath,. -o $@ $^

$(EXECUTABLE): wd_app.o $(SO)
	$(CC) $(CFLAGS) -Wl,-rpath,. -o $@ $^

clean:
	rm -rf *.o *.so $(EXECUTABLE) $(DAEMON)

obj_clean:
	rm -rf *.o 

rebuild: clean all obj_clean

.PHONY : clean obj_clean