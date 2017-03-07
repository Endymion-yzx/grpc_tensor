CXX = g++
CXXFLAGS = -std=c++11 -std=gnu++11 -pthread -I/usr/local/include
LDFLAGS += -L/usr/local/lib `pkg-config --libs grpc++ grpc`           \
		   -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed     \
		   -lprotobuf -lpthread -ldl

all: server client 

proto:
	protoc -I=. --cpp_out=. *.proto
	protoc -I=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` worker_service.proto

server: tensor_server.cc proto
	$(CXX) $(CXXFLAGS) -o server tensor_server.cc *.pb.cc $(LDFLAGS)

client: tensor_client.cc proto
	$(CXX) $(CXXFLAGS) -o client tensor_client.cc *.pb.cc $(LDFLAGS)

# multi: calc_client_multi.cc calc.grpc.pb.cc calc.pb.cc
# 	$(CXX) $(CXXFLAGS) -o multi calc_client_multi.cc calc.grpc.pb.cc calc.pb.cc $(LDFLAGS)

clean:
	rm *.pb.cc *.pb.h server client
