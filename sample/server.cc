#include "common.h"
#include "timer_queue.h"
#include "socket_group_netlink_impl.h"
#include "protobuf_message_server_impl.h"

int main(int argc, char** argv)
{
	using namespace csn;
	SocketGroup<SocketGroupNetlinkImpl> group{};
	try {
		std::shared_ptr<ProtobufMessageServerImpl> impl = std::make_shared<ProtobufMessageServerImpl>();
		std::shared_ptr<CacheDataCenter<CacheDataType>> center = impl->data_center();
		//insert some data for test,after update,data would be guaranteed no change during kDefaultExpireMillisecond seconds
		for (int i = 0; i < 50; ++i) {
			std::time_t timestamp;
			//insert cache data with:key i,data i,op_id i*i,callback ,
			center->update_op(i, std::to_string(50-i), i * i,
				[](csn::OpResult status, uint32_t op_id, std::time_t expire){
					LOG_OUT("update_op status %u op_id %u expire time %llu",status,op_id,expire);}, 
				&timestamp);
		}
		std::shared_ptr<MessageServer<UdpSocket>> server = std::make_shared<MessageServer<UdpSocket>>();
		server->set_message_impl(impl);
		server->initialize("*", 3824, "*", 0);
		group.register_socket(server);
	}
	CATCH_EXPTIONS;
	while (true) {
		try {
			csn::TimerQueue::get_timer_queue()->tick();
			group.listen(0.03);
		}
		CATCH_EXPTIONS;
	}
	return 0;
}
