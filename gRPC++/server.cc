#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include<map>
#include <exception> 
#include <iostream>
#include <thread>
 
#include <grpcpp/grpcpp.h>
#include "demo.grpc.pb.h"
#include "demo.pb.h"
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using ImgTransmit::ImgInfo;
using ImgTransmit::ImgInfo_Img;
typedef  ImgTransmit::Status My_Status;
using ImgTransmit::ImgInfo_ImgType;
using ImgTransmit::BaseName;
using ImgTransmit::Description;
typedef  ImgTransmit::Status My_Status;
using ImgTransmit::ImgDemo;
 
 
using Ms = std::chrono::milliseconds;
using Sec = std::chrono::seconds;
template <class UnitType=Ms>
using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock, UnitType>;
/*
		客户端，直接操作stub客户存根类对象进行通讯
		服务端，继承ImgDemo::Service，并实现相应的接口
		本例客户端，服务端均是同步(sync)实现
*/
// Logic and data behind the server's behavior.
class ImageServiceImpl final : public ImgDemo::Service {
public:
	::grpc::Status ImgUpload(ServerContext* context, ::grpc::ServerReaderWriter< ::ImgTransmit::ImgInfo,::ImgTransmit::ImgInfo>* reader) override {
		
		::ImgTransmit::ImgInfo info;
		int point_count = 0;
		int feature_count = 0;
		float distance = 0.0;
		// reader 接收客户端传来的一组图片
		if (resultList.size() > 0)
			resultList.clear();
		int error = -1;
		int count = 0;
		std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();
		while (reader->Read(&info)) {
			//挨个处理图片
			try {
				std::string name = info.name();
				google::protobuf::Map<google::protobuf::int32, ImgInfo_Img> maps = info.maps();
				int size = maps.size();
				google::protobuf::Map<google::protobuf::int32, ImgInfo_Img>::iterator it = maps.begin();
				for (; it != maps.end(); it++) {
					ImgInfo_Img img = it->second;
					int c = img.channel();
					int h = img.height();
					int w = img.width();
					std::string rowData = img.data();
					std::cout << "img size :" << h << "," << w << "," << c<<", data length:"<< rowData.size()<< std::endl;
					resultList[name] = info;
					
					std::string savePath("./picture/");
					std::ofstream  out(savePath+name, std::ios::out | std::ios::binary | std::ios::ate);
					out.write(rowData.c_str(), sizeof(char) * (rowData.size()));
					out.close();

					Mat imgg = imread("./picture/demo.png");
					Mat DstPic, edge, grayImage;

					DstPic.create(imgg.size(), imgg.type());
					cvtColor(imgg, grayImage, COLOR_BGR2GRAY);
					blur(grayImage, edge, Size(3, 3));

					//运行canny算子
					Canny(edge, edge, 3, 4, 3);
					//imwrite("Canny.jpg", edge);
					cout << "name: "+name <<endl;
					imwrite("./processed_image/"+name, edge);
					
				}
				count++;
 
			}
			catch (std::exception& e) {
				std::cout << "exception: " << e.what() << std::endl;
				error += 1;
			}
		}
		//告知客户端，消息是否收到
		std::chrono::system_clock::time_point end_time = std::chrono::system_clock::now();
		Sec secs =std::chrono::duration_cast<Sec>(end_time - start_time);
		Ms ms = std::chrono::duration_cast<Ms>(end_time - start_time);
		TimePoint<Sec> sec_time_point(secs);
		TimePoint<Ms> ms_time_point(ms);
		std::cout << count <<" images received success."<<std::endl;
		std::cout << "time cost is: "<< ms_time_point.time_since_epoch().count() <<" ms,  "
			<< sec_time_point.time_since_epoch().count()<<" seconds"<<std::endl;

		string path = "./processed_image/demo.png";
		ifstream imgreader(path, std::ifstream::in | std::ios::binary);

        imgreader.seekg(0, imgreader.end);  //将文件流指针定位到流的末尾
        int length = imgreader.tellg();
            
        imgreader.seekg(0, imgreader.beg);  //将文件流指针重新定位到流的开始
        char* buffer = (char*)malloc(sizeof(char) * length);
        imgreader.read(buffer, length);
    	imgreader.close();
            
        int crest = path.find_last_of('\\');
            
        ImgInfo_Img img_detail;
        ImgInfo_ImgType type = ImgInfo_ImgType::ImgInfo_ImgType_JPG;
        img_detail.set_height(224);
        img_detail.set_width(224);
        img_detail.set_channel(3);
        img_detail.set_type(type);
        img_detail.set_data(buffer, length);
        free(buffer);      
 
        ImgTransmit::ImgInfo infoo;
        infoo.set_name(path.substr(crest + 1, -1));
        google::protobuf::Map<google::protobuf::int32, ImgInfo_Img>* maps =infoo.mutable_maps();
        google::protobuf::MapPair<google::protobuf::int32, ImgInfo_Img> item(0, img_detail);
		maps->insert(item);
		//std::default_random_engine generator(seed);
        //std::this_thread::sleep_for(std::chrono::milliseconds(delay_distribution(generator)));
        
        reader->Write(infoo);
		if (error > 0) {
			return ::grpc::Status(::grpc::StatusCode::DATA_LOSS, "data received uncompletely.");
		}
		else
			return ::grpc::Status::OK;
	}
 
	::grpc::Status resImgFetched(::grpc::ServerContext* context, const ::ImgTransmit::BaseName* request, ::grpc::ServerWriter< ::ImgTransmit::ImgInfo>* writer)override {
		//const std::string& name = request->name();
		const google::protobuf::RepeatedPtrField<std::string>& names=request->name();
		int count=0, size = names.size();
		for (int i = 0; i < size;i++) {
			const std::string& singleName = names.Get(i);
			if (resultList.count(singleName) == 1) {
						::ImgTransmit::ImgInfo res = resultList[singleName];
						writer->Write(res);
						count--;
			}
			count++;
		}
		if(count<size)
			return ::grpc::Status::OK;
		else 
			return ::grpc::Status(::grpc::StatusCode::NOT_FOUND, "uncompleted reponse.");
	}
 
	::grpc::Status resDescFetched(::grpc::ServerContext* context, const ::ImgTransmit::BaseName* request, ::ImgTransmit::Description* response)override {
		const google::protobuf::RepeatedPtrField<std::string>& names = request->name();
		const google::protobuf::RepeatedPtrField<std::string>* rsp = response->mutable_desc();
		int count = 0, size = names.size();
		for (int i = 0; i < size; i++) {
			const std::string& singleName = names.Get(i);
			if (resultList.count(singleName) == 1) {
				std::string ss;
				const ::ImgTransmit::ImgInfo& singleInfo = resultList[singleName];
				response->add_desc(singleInfo.name());
				count--;
			}
			count++;
		}
		if (count < size)
			return ::grpc::Status::OK;
		else
			return ::grpc::Status(::grpc::StatusCode::NOT_FOUND, "uncompleted reponse.");
	}
private:
	std::map<std::string,::ImgTransmit::ImgInfo> resultList;
};
 
void RunServer() {
	std::string server_address("0.0.0.0:50057");
	ImageServiceImpl service;
	
	ServerBuilder builder;
    //Server-side Connection Management
    builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);//默认为0，在没有rpc待处理的情况下，不允许发PING帧
	builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,10000);//默认7200000，两个小时后发送PING帧
	builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);//默认20000，20秒后如果没收到PING ACK，就重发PING
	builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 10);//默认累计发了2个PING帧之后，必须发送一次带数据的帧才能继续发PING帧
	builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PING_STRIKES, 5);//默认为2，最多重发2次如果对方不响应，就断开连接
	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(&service);
	// Finally assemble the server.
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on " << server_address << std::endl;
 
	// Wait for the server to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	server->Wait();
}
 
 
int main(int argc, char** argv) {
 
	std::cout << "ready go to sleeping" << std::endl;
	RunServer();
 
	return 0;
}
 