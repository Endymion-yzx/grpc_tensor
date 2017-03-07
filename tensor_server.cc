// Implement the calc server
#include <iostream>
#include <memory>
#include <cmath>

#include <grpc++/grpc++.h>
#include <grpc/support/log.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>

#include "tensor.pb.h"            // Not needed
#include "worker.pb.h"            // Not needed
#include "worker_service.pb.h"    // Not needed
#include "worker_service.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using tensorflow::TensorProto;
using tensorflow::RecvTensorRequest;
using tensorflow::RecvTensorResponse;
using tensorflow::grpc::WorkerService;

// For the asynchronous version
using grpc::ServerAsyncResponseWriter;
using grpc::ServerCompletionQueue;

// The server
class ServerImpl final {
public:
	ServerImpl() {
		tensors = new TensorProto[10];
		for (int i = 0; i < 10; i++){
			for (int j = 0; j < 10; j++){
				tensors[i].add_double_val(i * j);
			}
		}
	}

	~ServerImpl() {
		server_->Shutdown();
		// Always shutdown the completion queue after the server.
		cq_->Shutdown();
	}

	// There is no shutdown handling in this code.
	void Run() {
		std::string server_address("127.0.0.1:50051");

		ServerBuilder builder;
		// Listen on the given address without any authentication mechanism.
		builder.AddListeningPort(server_address, 
				grpc::InsecureServerCredentials());
		// Register "service_" as the instance through which we'll 
		// communicate with clients. In this case it corresponds to an
		// *asychronous* service.
		builder.RegisterService(&service_);
		// Get hold of the completion queue used for the asynchronous
		// communication with the gRPC runtime.
		cq_ = builder.AddCompletionQueue();
		// Finally assemble the server.
		server_ = builder.BuildAndStart();
		std::cout << "Server listening on " << server_address << std::endl;

		// Proceed to the server's main loop;
		HandleRpcs();
	}

private:
	// Class encompassing the state and logic needed to serve a request.
	class CallData {
	public:
		// Take int the "service" instance (in this case representing an 
		// asynchronous server) and the completion queue "cq" used for 
		// asynchronous communication with the gRPC runtime
		CallData(WorkerService::AsyncService* service, ServerCompletionQueue* 
			cq, TensorProto* tensors) : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), tensors_(tensors) {
				// Invoke the serving logic right away
				Proceed();
			}

		void Proceed() {
			if (status_ == CREATE) {
				// Make this instance progress to the PROCESS state.
				status_ = PROCESS;

				// As part of the initial CREATE state, we *request* that 
				// the system start processing CalcArea requests. In this 
				// request, "this" acts are the tag uniquely identifying the
				// request (so that different CallData instances can serve
				// different requests concurrently), in this case the memory
				// address of this CallData instance.
				service_->RequestRecvTensor(&ctx_, &request, &responder_, 
						cq_, cq_, this);
			} else if (status_ == PROCESS) {
				// Spawn a new CallData instance to serve new clients while
				// we process the one for this CallData. This instance will
				// deallocate itself aa part of its FINISH state.
				new CallData(service_, cq_, tensors_);

				// The actual processing.
				// Fetch the tensor using "key" as an index
				long long key = request.key();
				response.set_allocated_tensor(new TensorProto(tensors_[key]));

				// And we are done! Let the gRPC runtime know we've 
				// finished, using memory address of this instance as the 
				// uniquely identifying tag for the event.
				status_ = FINISH;
				responder_.Finish(response, Status::OK, this);
			} else {
				GPR_ASSERT(status_ == FINISH);
				// Once in the FINISH state, deallocate ourselves (CallData)
				delete this;
			}
		}

	private:
		// The means of communication with the gRPC runtime for an 
		// asynchronous server.
		WorkerService::AsyncService* service_;
		// The producer-consumer queue where for asynchronous server 
		// notification.
		ServerCompletionQueue* cq_;
		// Context for the rpc, allowing to tweak aspects of it such as the 
		// use of compression, authentication, as well as to send metadata 
		// back to the client.
		ServerContext ctx_;

		// What we get from the client.
		RecvTensorRequest request;
		// What we send back to the client.
		RecvTensorResponse response;

		// The means to get back to the client.
		ServerAsyncResponseWriter<RecvTensorResponse> responder_;

		// Let's implement a tiny state machine with the following states.
		enum CallStatus { CREATE, PROCESS, FINISH };
		CallStatus status_;     // The current serving state.

		// An tensor array
		TensorProto* tensors_;
	};

	// This can be run in multiple threads if needed.
	void HandleRpcs() {
		// Spawn a new CallData instance to serve new clients.
		new CallData(&service_, cq_.get(), tensors);
		void* tag;    // uniquely identifies a request.
		bool ok;
		while (true) {
			// Block waiting to read the next event from the completion 
			// queue. The event is uniquely identified by its tag, which in
			// this case is the memory address of a CallData instance. 
			// The return value of Next should always be checked. This 
			// return value tells us whether there is any kind of event or 
			// cq_ is shutting down.
			GPR_ASSERT(cq_->Next(&tag, &ok));
			GPR_ASSERT(ok);
			static_cast<CallData*>(tag)->Proceed();
		}
	}

	std::unique_ptr<ServerCompletionQueue> cq_;
	WorkerService::AsyncService service_;
	std::unique_ptr<Server> server_;

	// An array of tensor
	TensorProto* tensors;
};

int main(){
	ServerImpl server;
	server.Run();

	return 0;
}
