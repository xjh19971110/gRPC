#include <chrono>
#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>
 
#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "demo.grpc.pb.h"
#include <opencv2/opencv.hpp>

using namespace cv;
 
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using ImgTransmit::ImgInfo_Img;
using ImgTransmit::ImgInfo_ImgType;
using ImgTransmit::ImgInfo;
using ImgTransmit::BaseName;
using ImgTransmit::Description;
typedef  ImgTransmit::Status My_Status;
using ImgTransmit::ImgDemo;
 
 
using Ms = std::chrono::milliseconds;
using Sec = std::chrono::seconds;
template <class UnitType = Ms>
using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock, UnitType>;
 
void parse_txt(const std::string & txt_path,std::vector<std::string>& content) {
    std::ifstream fp(txt_path);
    if (!fp.is_open())
    {
        std::cout << "can not open this file" << std::endl;
        return;
    }
    std::string line;
    while (getline(fp, line)) {
        // using printf() in all tests for consistency
        if (line.front() == '#'|| line.front() == ';')
            continue;
        printf("%s\n", line.c_str());
        content.push_back(line);
    }
    fp.close();
    return;
}
 
class ImageClient {
public:
    ImageClient(std::shared_ptr<Channel> channel)
        : stub_(ImgDemo::NewStub(channel)) {//stub底层对应着一个tcp socket，销毁stub就会断开tcp连接
    }
    void resImgFetched(const std::string& imgname) {
        BaseName query;
        ClientContext context;
        query.add_name(imgname);
        std::unique_ptr<ClientReader<ImgTransmit::ImgInfo> > reader(stub_->resImgFetched(&context, query));
        ImgTransmit::ImgInfo info;
        //info 将会在服务端的resImgFetched中被处理
        while (reader->Read(&info)) {//将流读完
            std::cout << "Found feature " << info.name() << std::endl;
        }
        Status status = reader->Finish();
        if (status.ok()) {
            std::cout << " result fetch rpc succeeded."<< std::endl;
        }
        else {
            std::cout << "result fetch rpc failed." << std::endl;
        }
    }
 
    void resDescFetched(const std::string& imgname) {
        BaseName query;
        Description res;
        ClientContext context;
        query.add_name(imgname);
        Status status = stub_->resDescFetched(&context, query, &res);
        if (!status.ok()) {
            std::cout << "GetFeature rpc failed." << std::endl;
            return;
        }
        auto descs = res.desc();
        int size = descs.size();
        for (int i = 0; i < size; i++) {
            const std::string& description = descs.Get(i);
            std::cout << "get success: " << description << std::endl;
        }
        return;
    }
 
    void ImgUpload(const std::vector<std::string>& img_list) {
        My_Status stats;
        ClientContext context;
        const int kPoints = 10;
        unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
 
        std::default_random_engine generator(seed);
        std::uniform_int_distribution<int> delay_distribution(500, 1500);
        
        //这个stats将会在服务端的ImgUpload被处理
        std::unique_ptr<ClientReaderWriter<::ImgTransmit::ImgInfo,::ImgTransmit::ImgInfo> > writer(stub_->ImgUpload(&context));
        std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();
        for (int i = 0; i < img_list.size(); i++) {
            std::string path = img_list.at(i);
            
            std::ifstream imgreader(path, std::ifstream::in | std::ios::binary);
            if(!imgreader.is_open())
                continue;
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
 
            ImgTransmit::ImgInfo info;
            info.set_name(path.substr(crest + 1, -1));
            google::protobuf::Map<google::protobuf::int32, ImgInfo_Img>* maps =info.mutable_maps();
            google::protobuf::MapPair<google::protobuf::int32, ImgInfo_Img> item(i, img_detail);
            //maps->operator[](i)= img_detail;
            maps->insert(item);
 
            if (!writer->Write(info)) {
                // Broken stream.
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_distribution(generator)));
        }
        writer->WritesDone();
        std::chrono::system_clock::time_point end_time = std::chrono::system_clock::now();
        //Status status = writer->Finish();
        
       /* if (status.ok()) {
            std::cout << "ImgUpload finished  with code: " << stats.code() <<  std::endl;
            Ms ms = std::chrono::duration_cast<Ms>(end_time - start_time);
            TimePoint<Ms> ms_time_point(ms);
            std::cout << "time cost is: " << ms_time_point.time_since_epoch().count() << " ms."<< std::endl;
        }
        else {
            std::cout << "ImgUpload rpc failed with code: "<<stats.code()<< std::endl;
        }*/
        ::ImgTransmit::ImgInfo info;
		int point_count = 0;
		int feature_count = 0;
		float distance = 0.0;
		// reader 接收客户端传来的一组图片
		if (resultList.size() > 0)
			resultList.clear();
		int error = -1;
		int count = 0;
		
		while (writer->Read(&info)) {
			//挨个处理图片
			try {
				std::string namee = info.name();
                int index = namee.find_last_of("//");
                std::string name = namee.substr(index);
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
					
					std::string savePath("./client_pic/");
					std::ofstream  out(savePath+name, std::ios::out | std::ios::binary | std::ios::ate);
					out.write(rowData.c_str(), sizeof(char) * (rowData.size()));
					out.close();

                    Mat m2 = imread(savePath+name);
                    imshow("after",m2);
                    cvWaitKey(0);


					
				}
				count++;
 
			}
			catch (std::exception& e) {
				std::cout << "exception: " << e.what() << std::endl;
				error += 1;
			}
		}
    }
 
private:
    std::unique_ptr<ImgDemo::Stub> stub_;
	std::map<std::string,::ImgTransmit::ImgInfo> resultList;
};
 
int main(int argc, char** argv) {
    ImageClient client(grpc::CreateChannel("localhost:50057",grpc::InsecureChannelCredentials()));
    std::cout << "-------------- upload image --------------" << std::endl;
    std::cout << "please assign the img list file:" << std::endl;
    std::vector<std::string> imglist;
    imglist.push_back("demo.png");

    Mat m1 = imread("demo.png");
    imshow("before",m1);
    cvWaitKey(10);
    

    client.ImgUpload(imglist);
    /*std::cout << "-------------- fetch image result --------------" << std::endl;
    client.resImgFetched("98bb90c44f55aaeeae417d8233226785.jpg");
    std::cout << "-------------- fetch description result --------------" << std::endl;
    client.resDescFetched("98bb90c44f55aaeeae417d8233226785.jpg");
    std::cin.get();*/
    return 0;
}