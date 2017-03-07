// Calc client
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include "calc.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using calc::Calc;
using calc::Point;
using calc::Circle;
using calc::Area;
using calc::Circum;

// For asynchronous version
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

class CalcClient {
public:
	CalcClient(std::shared_ptr<Channel> channel) : 
		stub_(Calc::NewStub(channel)) {}

	// Assembles the client's payload, sends it and presents the response 
	// back from the server
	double CalcAreaAsync(double radius){
		// Data we are sending to the server
		Circle circle;
		circle.set_radius(radius);

		// Container for the data we expect from the server
		Area area;

		// Context for the client. It could be used to convey extra 
		// information to the server and/or tweak certain RPC behavios.
		ClientContext context;

		// The producer-consumer queue we use to communicate asynchronously
		// with the gRPC runtime.
		CompletionQueue cq;

		// Storage for the status of the RPC upon completion.
		Status status;

		// stub_->AsyncCalcArea() performs the RPC call, returning an 
		// instance we store in "rpc". Because we are using the asynchronous
		// API, we need to hold on to the "rpc" instance in order to get 
		// updates on the ongoing RPC.
		std::unique_ptr<ClientAsyncResponseReader<Area> > rpc(
				stub_->AsyncCalcArea(&context, circle, &cq));

		// Request that, upon completion of the RPC, "area" be updated with 
		// the server's response; "status" with the indication of whether 
		// the operation was successful. Tag the request with the integer 1.
		rpc->Finish(&area, &status, (void*)1);
		void* got_tag;
		bool ok = false;
		// zyang: Sending request is non-blocking while reading cq is blocking?
		// Block until the next result is available in the completion queue
		// "cq". The return value of Next should always be checked. This 
		// return value tells us whether there is any kind of event or the 
		// cq_ is shutting down.
		GPR_ASSERT(cq.Next(&got_tag, &ok));

		// Verify that the result from "cq" corresponds, by its tag, our
		// previous request.
		GPR_ASSERT(got_tag == (void*)1);
		// ... and that the request was completed successfully. Note that 
		// "ok" corresponds solely to the request for updates introduced by
		// Finish().
		GPR_ASSERT(ok);

		// Act upon the status of the actual RPC.
		if (status.ok()) {
			return area.value();
		} else {
			std::cout << "RPC failed" << std::endl;
			exit(-1);
		}
	}

private:
	std::unique_ptr<Calc::Stub> stub_;
};

int main(int argc, char** argv){
	CalcClient client(grpc::CreateChannel("localhost:50051", 
				grpc::InsecureChannelCredentials()));
	std::cout << "---------------- CalcArea ----------------" << std::endl;
	for (int i = 1; i <= 100000; i++){
		double r = i;
		std::cout << "radius = " << r << std::endl;
		double area = client.CalcAreaAsync(r);
		std::cout << "area = " << area << std::endl;
	}

	return 0;
}
