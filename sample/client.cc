#include "common.h"
#include "timer_queue.h"
#include "socket_group_netlink_impl.h"
#include "protobuf_message_client_impl.h"

using namespace csn;
int main(int argc, char** argv) {
	csn::SocketGroup<SocketGroupNetlinkImpl> group{};
	std::shared_ptr<ProtobufMessageClientImpl> impl = std::make_shared<ProtobufMessageClientImpl>(1, 452);
	std::shared_ptr<MessageClient<UdpSocket>> client=std::make_shared<MessageClient<UdpSocket>>();
	client->set_message_impl(impl);
	try {
		client->initialize("*", 3823, "127.0.0.1", 3824);
		group.register_socket(client);
	}
	CATCH_EXPTIONS;
	int count = 0;
	// update a piece of data which cache id is 2
	client->update_cache_async(2, std::to_string(845745) , [](csn::OpResult result, std::time_t expire,
		uint32_t cache_id, CacheDataType cache_data) {
			LOG_OUT("result %u expire %d ms cache_id %d data %s", result,
				csn::expire_milliseconds_of_timestamp(expire), expire, cache_data.c_str());
		});
	// read a piece of data which cache id is 2
	client->read_cache_async(2,[](csn::OpResult result, std::time_t expire,
		uint32_t cache_id, CacheDataType cache_data) {
			LOG_OUT("result %u expire %d ms cache_id %d data %s", result,
				csn::expire_milliseconds_of_timestamp(expire), expire, cache_data.c_str());
		});
	while (true){
		++count;
		try {
#if 0
			if (count % 30==0) {

				client->read_cache_async(count%50, [](csn::OpResult result, std::time_t expire,
					uint32_t cache_id, CacheDataType cache_data) {
						LOG_OUT("result %u expire %dms cache_id %d data %s", result,
							csn::expire_milliseconds_of_timestamp(expire), expire, cache_data.c_str());
					});
			}
#endif
			group.listen(0.03);
		}
		CATCH_EXPTIONS;
	}
}