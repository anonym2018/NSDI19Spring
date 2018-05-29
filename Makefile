main : RTSSmain.o RangeTupleSpaceSearch.o cmap.o
	g++ -std=c++14 -g -o main RTSSmain.o RangeTupleSpaceSearch.o cmap.o
# -g 是编译参数  -w 是忽略警告
RTSSmain.o : ElementaryClasses.h hash.h cmap.h RangeTupleSpaceSearch.h RTSSmain.cpp
	g++ -g -std=c++14 -c RTSSmain.cpp 
RangeTupleSpaceSearch.o : ElementaryClasses.h hash.h cmap.h RangeTupleSpaceSearch.h RangeTupleSpaceSearch.cpp
	g++ -g -std=c++14 -c RangeTupleSpaceSearch.cpp 
cmap.o : ElementaryClasses.h hash.h cmap.h cmap.cpp 
	g++ -g -std=c++14 -c cmap.cpp 

clean:
	rm *.o main





