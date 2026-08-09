#ifndef USERINFO_H
#define USERINFO_H
#include <iostream>

struct UserInfo
{
    int user_id;

    std::string user_name;

    std::string avatar;

    std::string create_time;

    void print() const
    {
        std::cout<<"id:\t\t"<<user_id<<std::endl;
        std::cout<<"name:\t\t"<<user_name<<std::endl;
        std::cout<<"avatar:\t\t"<<avatar<<std::endl;
        std::cout<<"create_time:\t\t"<<create_time<<std::endl;
    }

};

struct UserStat
{
    int post_count;

    int comment_count;

    int like_count;

    std::string create_time;
};
#endif