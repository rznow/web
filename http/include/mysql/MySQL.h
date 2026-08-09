#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <unordered_map>

class UserInfo;
class UserStat;
class Post;
class Comment;
class MySQL
{
private:
    MYSQL* conn;
    
public:
    MySQL();
    ~MySQL();

    bool connect(
        const std::string& host,
        const std::string& user,
        const std::string& password,
        const std::string& db,
        int port);

    bool query(const std::string& sql);

    bool reconnect(
        const std::string& host,
        const std::string& user,
        const std::string& password,
        const std::string& db,
        int port);

    int loginSQL(const std::string& name, const std::string& password, UserInfo& user);

    int registerSQL(const std::string& name, const std::string& password);

    bool modAvatar(int user_id, const std::string& avatarUrl);

    bool getAvatar(int user_id, std::string& avatarUrl);

    int savePost(Post& p);

    void saveComment(Comment& c);

    MYSQL* get();

    void getAllPostIds(std::unordered_map<std::string, std::string>& members);
    void getPosts(std::vector<Post>& posts, size_t size,size_t offset);
    void getPosts(
        const std::vector<int>& ids, 
        std::vector<Post>& posts, 
        const std::vector<int>& missPos);

    void getRootComments(
        std::vector<int>& comments_id, 
        size_t post_id, size_t size, size_t offset);

    void getComments(
        const std::vector<int>& ids, 
        std::vector<Comment>& comments, 
        const std::vector<int>& missCom);

    void getComments(   //获取全部评论
        std::vector<Comment>& comments, 
        size_t post_id);
    void getComments(   //获取根评论下评论
        std::vector<Comment>& comments, 
        size_t post_id, 
        std::vector<int>& rootComments);

    bool getPost(int post_id, Post& p);

    bool delPost(int post_id);

    void like(int post_id, int user_id, bool liked);

    bool liked(int post_id, int user_id);

    bool likes(int post_id, int& like_count);         //获取点赞数

    bool getLikes(int post_id, std::vector<int>& likes);    //获取喜欢对应帖子的全部用户

    int view(int post_id);

    bool checkPost(int post_id, int user_id);

    bool modPost(int post_id, std::string title, std::string content);

    bool load(int post_id, const std::unordered_map<std::string, std::string>& fields);

    bool getPostCount(int user_id, int& count);
    bool getCommentCount(int user_id, int& count);
    bool getLikeCount(int user_id, int& count);

    // bool getCreateTime(int user_id, std::string& time);
    bool getUserStat(int user_id, UserStat& stat);
};