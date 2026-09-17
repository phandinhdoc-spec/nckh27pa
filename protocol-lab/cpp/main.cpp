#include "framing.hpp"
#include <iostream>
#include <sstream>
lab::Bytes unhex(const std::string& s) {
    lab::Bytes b;
    if(s=="-") return b;
    if(s.size()%2) throw std::invalid_argument("odd hex");
    for(std::size_t i=0;i<s.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
    return b;
}
std::string hex(const lab::Bytes& b) {
    const char* digits="0123456789abcdef"; std::string s;
    for(auto v:b) { s+=digits[v>>4]; s+=digits[v&15]; } return s;
}
int main() {
    lab::Reassembler r; std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream in(line); char op; in>>op;
        if(op=='E') {
            int mtu,kind; std::uint64_t id; std::string h; in>>mtu>>kind>>id>>h;
            auto frames=lab::encode(mtu,kind,id,unhex(h));
            if(!frames) { std::cout<<"INVALID\n"; continue; }
            std::cout<<"FRAMES "; bool first=true;
            for(const auto& f:*frames) { if(!first) std::cout<<','; first=false; std::cout<<hex(f); }
            std::cout<<'\n'; continue;
        }
        lab::Result result;
        if(op=='R') { std::int64_t now; std::uint64_t gen; int kind; std::string h; in>>now>>gen>>kind>>h; result=r.receive(now,gen,kind,unhex(h)); }
        else if(op=='T') { std::int64_t now; in>>now; result=r.tick(now); }
        else if(op=='D') result=r.disconnect();
        else if(op=='M') { std::cout<<sizeof(lab::Reassembler)<<'\n'; continue; }
        else throw std::invalid_argument("unknown host harness command");
        std::cout<<result.status;
        if(!result.payload.empty()) std::cout<<' '<<hex(result.payload);
        std::cout<<' '<<r.active()<<' '<<r.generation()<<'\n';
    }
}
