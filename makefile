LDFLAGS += -lX11
CXXFLAGS += -Wall

SRC=wintag.cpp
OBJS=$(patsubst %.cpp,%.o,$(SRC))
BINARY=wintag

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

$(BINARY): $(OBJS)
	g++ $(LDFLAGS) -o $@ $^

clean:
	rm -rf $(OBJS) $(BINARY)

.PHONY: clean
