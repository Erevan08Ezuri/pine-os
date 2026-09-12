#pragma once
#include <cstdint>
#include <string_view>

namespace Pine {
inline std::uint32_t nextUtf8(std::string_view text,std::size_t& position) {
    if(position>=text.size())return 0;
    const auto first=static_cast<unsigned char>(text[position++]);
    if(first<0x80)return first;
    const int length=first>=0xc2&&first<=0xdf?2:first>=0xe0&&first<=0xef?3:first>=0xf0&&first<=0xf4?4:0;
    if(!length||position+length-1>text.size())return 0xfffd;
    std::uint32_t code=first&((1u<<(7-length))-1);
    for(int i=1;i<length;++i){const auto byte=static_cast<unsigned char>(text[position+i-1]);if((byte&0xc0)!=0x80)return 0xfffd;code=(code<<6)|(byte&0x3f);}
    position+=length-1;
    if((length==2&&code<0x80)||(length==3&&code<0x800)||(length==4&&code<0x10000)||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return 0xfffd;
    return code;
}
}
