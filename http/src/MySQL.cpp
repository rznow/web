#include "mysql/MySQL.h"
#include "common/UserInfo.h"
#include <mysql/mysql.h>
#include <common/Post.h>
#include <common/Comment.h>
#include <iostream>
#include <mysql/Statement.h>

MySQL::MySQL()
{
    // 初始化MySQL对象
    conn = mysql_init(NULL);

}

MySQL::~MySQL()
{
    // 关闭数据库连接
    mysql_close(conn);
    conn = nullptr;
}

bool MySQL::connect(const std::string& host,
        const std::string& user,
        const std::string& password,
        const std::string& db,
        int port)
{
    //mysql_real_connect 用于链接数据库
    return mysql_real_connect(
               conn,
               host.c_str(),
               user.c_str(),
               password.c_str(),
               db.c_str(),
               port,
               nullptr,
               0
           ) != nullptr;
}


bool MySQL::query(const std::string& sql)
{
    int ret = mysql_query(conn, sql.c_str());

    if(ret != 0)
    {
        // std::cout << "SQL failed: " << sql << std::endl;
        // std::cout << "Error: " << mysql_error(conn) << std::endl;
        return false;
    }
    // std::cout << "SQL success: " << sql << std::endl << std::endl;
    return true;
}

bool MySQL::reconnect(
        const std::string& host,
        const std::string& user,
        const std::string& password,
        const std::string& db,
        int port)
{
    // 先释放旧连接
    if(conn != nullptr)
    {
        mysql_close(conn);
        conn = nullptr;
    }

    // 重新初始化
    conn = mysql_init(nullptr);
    if(conn == nullptr)
    {
        std::cout << "mysql_init failed\n";
        return false;
    }

    connect(
        host,
        user,
        password,
        db,
        port);

    if(conn == nullptr)
    {
        std::cout << "mysql_real_connect failed: "
                  << mysql_error(conn) << std::endl;
        return false;
    }

    return true;
}
/*
    *未注册返回-1
    *密码错误返回0
    *登录成功返回1
*/
int MySQL::loginSQL(const std::string& name, const std::string& password, UserInfo& user)
{
    Statement stmt(conn,R"(
        SELECT 
        user_id,
        password,
        avatar,
        DATE_FORMAT(create_time,'%Y-%m-%d %H:%i:%s') AS create_time
        FROM user_info
        WHERE user_name=?
    )");

    stmt.bindString(0,name);

    if(!stmt.execute())
        return -1;

    stmt.storeResult();

    

    // stmt.bindResultInt(0,id);
    // stmt.bindResultString(1,dbPassword,sizeof(dbPassword));

    if(!stmt.fetch())
        return -1;
    
    StatementRow& row = stmt.row();
    int id = row.getInt(0);
    std::string dbPassword = row.getString(1);

    if(password != dbPassword)
        return 0;

    user.user_id=id;
    user.user_name=name;
    user.avatar            = row.getString(2);
    
    return 1;
}
/*
    *异常返回-1
    *已注册返回0
    *注册成功返回1
*/
int MySQL::registerSQL(const std::string& name, const std::string& password)
{
    Statement stmt(conn, R"(
        SELECT user_id
        FROM user_info 
        WHERE user_name=?;
    )");

    stmt.bindString(0, name);

    if(!stmt.execute())
        return -1;

    stmt.storeResult();

    // stmt.bindResultInt(0, id);

    if(stmt.fetch())        //用户已注册
    {
        int id=stmt.row().getInt(0);
        std::cout << "user "<<id<<" already exists!" << std::endl;
        return 0;
    }

    Statement register_stmt(conn, R"(
        INSERT INTO 
        user_info
        (
        user_name, 
        password
        ) 
        VALUES(
        ?,
        ?
        );
    )");

    register_stmt.bindString(0, name);
    register_stmt.bindString(1, password);

    if(!register_stmt.execute())
    {
        std::cout << "register failed!" << std::endl;
        return -1;
    }

    std::cout << "register success!" << std::endl;
    return 1;
}

bool MySQL::modAvatar(int user_id, const std::string& avatarUrl)
{
    Statement stmt(conn, R"(
        UPDATE user_info
        SET avatar=?
        WHERE user_id=?;
    )");

    stmt.bindString(0, avatarUrl);
    stmt.bindInt(1, user_id);

    if(!stmt.execute())
        return false;

    return true;
}

bool MySQL::getAvatar(int user_id, std::string& avatarUrl)
{
    Statement stmt(conn, R"(
        SELECT avatar
        FROM user_info
        WHERE user_id=?;
    )");

    stmt.bindInt(0, user_id);

    if(!stmt.execute())
        return false;
    stmt.storeResult();

    if(!stmt.fetch())
        return -1;
    
    avatarUrl = stmt.row().getString(0);

    return true;
}

MYSQL* MySQL::get()
{
    return conn;
}

void MySQL::getAllPostIds(std::unordered_map<std::string, std::string>& members)
{
    std::string sql = R"(
        SELECT 
        post_id,
        create_time
        FROM posts
        WHERE deleted = 0;
    )";

    if (!query(sql))    return;

    MYSQL_RES* res = mysql_store_result(conn);

    if (!res)   return;

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res)) != nullptr)
    {
        members[row[0]] = row[1];
    }

    mysql_free_result(res);
}

int MySQL::savePost(Post& p)
{
    Statement stmt(conn, R"(
        INSERT INTO 
        posts
        (
        user_id, 
        title, 
        content
        )
        VALUES( 
        ?,
        ?,
        ?
        );
    )");

    stmt.bindInt(0, p.user_id);
    stmt.bindString(1, p.title);
    stmt.bindString(2, p.content);
    
    if(!stmt.execute())
    {
        std::cout << "save post failed\n";
        return -1;
    }
    
    p.post_id = stmt.insertId();

    //从mysql中获取表项(帖子)创建时间
    std::string create_time_sql = "SELECT create_time FROM posts WHERE post_id=" + std::to_string(p.post_id);
    query(create_time_sql);
    MYSQL_RES* res = mysql_store_result(conn);
    if(!res) return -1;
    // size_t t = mysql_num_fields(res);
    MYSQL_ROW row = mysql_fetch_row(res);
    if(row == nullptr)
    {
        mysql_free_result(res);
        return -1;
    }
    // std::cout<<"mysql_num_fields:"<<t<<std::endl;
    p.create_time = row[0];
    p.like_count = 0;
    p.view_count = 0;
    
    mysql_free_result(res);
    return p.post_id;
}

void MySQL::saveComment(Comment& c)
{
    Statement stmt(conn, R"(
        INSERT INTO 
        comments
        (
        post_id,
        user_id,
        parent_id,
        root_comment_id,
        reply_user_id,
        content
        )
        VALUES
        (
        ?,
        ?,
        ?,
        ?,
        ?,
        ?
        );
    )");

    stmt.bindInt(0, c.post_id);
    stmt.bindInt(1, c.user_id);
    stmt.bindInt(2, c.parent_id);
    stmt.bindInt(3, c.root_comment_id);
    if(c.reply_user_id==-1) stmt.bindNull(4);
    else stmt.bindInt(4, c.reply_user_id);
    stmt.bindString(5, c.content);

    if(!stmt.execute())
    {
        // std::cout << "save Comment failed\n";
        return;
    }
        

    int comment_id = stmt.insertId();

    if(c.parent_id == 0)
    {
        std::string sql =
        "UPDATE comments "
        "SET root_comment_id = " + std::to_string(comment_id) +
        " WHERE comment_id = "
        + std::to_string(comment_id)
        + ";";

        query(sql);
    }

    std::string sql = R"(
    SELECT
        c.comment_id,
        c.post_id,
        c.user_id,

        c.parent_id,
        c.root_comment_id,
        c.reply_user_id,

        u.user_name,
        u2.user_name AS reply_name,
        u.avatar,
        c.content,
        c.create_time
    FROM comments c
    JOIN user_info u
    ON c.user_id=u.user_id

    LEFT JOIN user_info u2
    ON c.reply_user_id=u2.user_id

    WHERE c.comment_id=)" + std::to_string(comment_id) +
    " ; ";

    query(sql);
    MYSQL_RES* res = mysql_store_result(conn);
    if(!res) return;
    MYSQL_ROW row = mysql_fetch_row(res);
    if(!row) return;
    c = Comment(row);
}

void MySQL::getPosts(std::vector<Post>& posts,size_t size,size_t offset)
{
    std::string sql = R"(
    SELECT
        p.post_id,
        p.user_id,
        u.user_name,
        p.title,
        p.content,
        p.like_count,
        p.comment_count,
        p.view_count,
        p.create_time
    FROM posts p
    INNER JOIN user_info u
    ON p.user_id = u.user_id
    WHERE p.deleted = 0
    ORDER BY p.create_time DESC
    LIMIT )" 
    + std::to_string(size) +
    " OFFSET " +
    std::to_string(offset) +
    ";";
    

    if(!query(sql)) return;
    MYSQL_RES * res = mysql_store_result(conn);

    if(res == nullptr) return;

    MYSQL_ROW row;
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        Post p(row);
        posts.push_back(std::move(p));
    }
    
}

void MySQL::getPosts(
    const std::vector<int>& ids, 
    std::vector<Post>& posts, 
    const std::vector<int>& missPos)
{
    if (ids.empty()) return;

    std::string ids_str;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) ids_str += ",";
        ids_str += std::to_string(ids[i]);
    }

    // std::string sql = R"(
    // SELECT
    //     p.post_id,
    //     p.user_id,
    //     u.user_name,
    //     p.title,
    //     p.content,
    //     p.like_count,
    //     p.comment_count,
    //     p.view_count,
    //     p.create_time
    // FROM posts p
    // INNER JOIN user_info u
    // ON p.user_id = u.user_id
    // WHERE p.deleted = 0
    // AND p.post_id IN ()" + ids_str + ");";
    std::string sql = R"(
    SELECT
        p.post_id,
        p.user_id,
        u.user_name,
        p.title,
        p.content,
        p.like_count,
        p.comment_count,
        p.view_count,
        p.create_time
    FROM posts p
    INNER JOIN user_info u
    ON p.user_id = u.user_id
    WHERE p.deleted = 0
    AND p.post_id IN ()" + ids_str + ")"
    "ORDER BY FIELD(p.post_id," + ids_str + ");";



    if(!query(sql)) return;
    MYSQL_RES * res = mysql_store_result(conn);
    if(res == nullptr) return;
    MYSQL_ROW row;
    for(auto &i: missPos)
    {
        if((row = mysql_fetch_row(res)) != nullptr)
        {
            Post p(row);
            posts[i] = std::move(p);
        }else return;
    }
}

void MySQL::getRootComments(std::vector<int>& comments_id, size_t post_id, size_t size, size_t offset)
{
    Statement stmt(conn, R"(
    SELECT
        c.comment_id
    FROM comments c
    WHERE c.post_id=?
    AND parent_id = 0
    ORDER BY
    create_time DESC
    LIMIT ?
    OFFSET ?;
    )"
    );

    stmt.bindInt(0, post_id);
    stmt.bindInt(1, size);
    stmt.bindInt(2, offset);

    if(!stmt.execute())
    {
        std::cout<<"get rootComments failrd!"<<std::endl;
        return;
    }

    stmt.storeResult();

    while(stmt.fetch())
    {
        comments_id.push_back(stmt.row().getInt(0));
    }
}

void MySQL::getComments(
    const std::vector<int>& ids, 
    std::vector<Comment>& comments, 
    const std::vector<int>& missCom)
{
    if (ids.empty()) return;

    std::string ids_str;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) ids_str += ",";
        ids_str += std::to_string(ids[i]);
    }

    std::string sql = R"(
        SELECT
            c.comment_id,
            c.post_id,
            c.user_id,

            c.parent_id,
            c.root_comment_id,
            c.reply_user_id,
            u.avatar,

            u.user_name,
            u2.user_name AS reply_name,
            u.avatar,
            c.content,
            DATE_FORMAT(c.create_time,'%Y-%m-%d %H:%i:%s') AS create_time
        FROM comments c
        JOIN user_info u
        ON c.user_id = u.user_id

        LEFT JOIN user_info u2
        ON c.reply_user_id = u2.user_id

        WHERE c.comment_id IN (
        )" + ids_str + R"(
        )
        ORDER BY FIELD(c.comment_id,
        )" + ids_str + ");";

    if(!query(sql)) return;
    MYSQL_RES * res = mysql_store_result(conn);
    if(res == nullptr) return;
    MYSQL_ROW row;
    for(auto &i: missCom)
    {
        if((row = mysql_fetch_row(res)) != nullptr)
        {
            Comment c(row);
            comments[i] = std::move(c);
        }else return;
    }
}

void MySQL::getComments(std::vector<Comment>& comments, size_t post_id)
{
    std::string sql = R"(
    SELECT
        c.comment_id,
        c.post_id,
        c.user_id,

        c.parent_id,
        c.root_comment_id,
        c.reply_user_id,

        u.user_name,
        u2.user_name AS reply_name,
        u.avatar,
        c.content,
        c.create_time
    FROM comments c
    JOIN user_info u
    ON c.user_id=u.user_id

    LEFT JOIN user_info u2
    ON c.reply_user_id=u2.user_id

    WHERE c.post_id=)" + std::to_string(post_id) +
    R"(
    ORDER BY
    parent_id,
    create_time;)";
    

    if(!query(sql)) return;
    MYSQL_RES * res = mysql_store_result(conn);

    if(res == nullptr) return;

    MYSQL_ROW row;
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        comments.emplace_back(row);
    }
}

void MySQL::getComments(std::vector<Comment>& comments, size_t post_id, std::vector<int>& rootComments)
{
    if (rootComments.empty()) return;

    std::string ids_str;
    for (size_t i = 0; i < rootComments.size(); ++i) {
        if (i > 0) ids_str += ",";
        ids_str += std::to_string(rootComments[i]);
    }

    std::string sql = R"(
        SELECT
            c.comment_id,
            c.post_id,
            c.user_id,

            c.parent_id,
            c.root_comment_id,
            c.reply_user_id,

            u.user_name,
            u2.user_name AS reply_name,
            u.avatar,
            c.content,
            DATE_FORMAT(c.create_time,'%Y-%m-%d %H:%i:%s') AS create_time
        FROM comments c
        JOIN user_info u
        ON c.user_id = u.user_id

        LEFT JOIN user_info u2
        ON c.reply_user_id = u2.user_id

        WHERE c.root_comment_id IN (
        )" + ids_str + R"(
        )ORDER BY FIELD(c.root_comment_id,
        )" + ids_str + ");";

    if(!query(sql)) return;
    MYSQL_RES * res = mysql_store_result(conn);
    if(res == nullptr) return;
    MYSQL_ROW row;
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        Comment c(row);
        comments.emplace_back(std::move(c));
    }
}

bool MySQL::getPost(int post_id, Post& p)
{
    std::string sql = R"(
    SELECT
        p.post_id,
        p.user_id,
        u.user_name,
        p.title,
        p.content,
        p.like_count,
        p.comment_count,
        p.view_count,
        p.create_time
    FROM posts p
    INNER JOIN user_info u
    ON p.user_id = u.user_id
    where p.post_id = )" 
    + std::to_string(post_id) +
    " and p.deleted <> 1"+
    ";";

    if(!query(sql)) return false;
    MYSQL_RES * res = mysql_store_result(conn);

    if(res == nullptr) return false;

    MYSQL_ROW row;
    row = mysql_fetch_row(res);
    
    p = Post(row);
   
    // p.print();

    return true;
}

bool MySQL::delPost(int post_id)
{
    std::string sql = R"(
    UPDATE
    posts
    SET deleted = 1
    where post_id = )" 
    + std::to_string(post_id) +
    ";";
    if(!query(sql)) return false;

    return true;
}

void MySQL::like(int post_id, int user_id, bool liked)
{
    // std::string sql = R"(
    // SELECT * FROM
    // post_like
    // where post_id = )" 
    // + std::to_string(post_id) +
    // " and user_id = "
    // + std::to_string(user_id) +
    // ";";

    // if(!query(sql)) return -1;
    // MYSQL_RES* res = mysql_store_result(conn);
    // if(res == nullptr) return -1;
    // MYSQL_ROW row = mysql_fetch_row(res);
    std::string sql;
    if(liked) //取消点赞
    {
        sql = R"(
            DELETE FROM post_like
            WHERE user_id = )" +
            std::to_string(user_id) +
            " and post_id = " + 
            std::to_string(post_id) + 
            ";";
        if(!query(sql)) return;
        // sql = R"(
        //     UPDATE posts
        //     SET like_count =
        //     like_count - 1
        //     WHERE post_id = )" + 
        //     std::to_string(post_id) + 
        //     ";";
        // if(!query(sql)) return;
    }else
    {
        sql = R"(
            INSERT INTO post_like
            (user_id, post_id)
            VALUES( )" + 
            std::to_string(user_id) +
            " , " + 
            std::to_string(post_id) + 
            ");";
        if(!query(sql)) return;
        // sql = R"(
        //     UPDATE posts
        //     SET like_count =
        //     like_count + 1
        //     WHERE post_id = )" + 
        //     std::to_string(post_id) + 
        //     ";";
        // if(!query(sql)) return;
        
    }

    // sql = R"(
    //     SELECT like_count 
    //     FROM posts
    //     WHERE post_id = )" + 
    //     std::to_string(post_id) + 
    //     ";";
    // if(!query(sql)) return -1;

    // res = mysql_store_result(conn);
    // row = mysql_fetch_row(res);

    // return std::stoi(row[0]);
}

bool MySQL::liked(int post_id, int user_id)
{
    std::string sql = R"(
    SELECT * FROM
    post_like
    where post_id = )" 
    + std::to_string(post_id) +
    " and user_id = "
    + std::to_string(user_id) +
    ";";

    if(!query(sql)) return false;
    MYSQL_RES* res = mysql_store_result(conn);
    if(res == nullptr) return false;
    MYSQL_ROW row = mysql_fetch_row(res);

    if(row == nullptr) return false;
    return true;
}

bool MySQL::likes(int post_id, int& like_count) //获取点赞数
{
    std::string sql = R"(
    SELECT like_count FROM
    posts
    where post_id = )" 
    + std::to_string(post_id) +
    ";";

    if(!query(sql)) return false;
    MYSQL_RES* res = mysql_store_result(conn);
    if(res == nullptr) return false;
    MYSQL_ROW row = mysql_fetch_row(res);

    if(row == nullptr) return false;
    return true;
}

bool MySQL::getLikes(int post_id, std::vector<int>& likes)
{
    std::string sql = R"(
    SELECT user_id FROM
    post_like
    where post_id = )" 
    + std::to_string(post_id) + 
    ";";

    if(!query(sql)) return false;
    MYSQL_RES* res = mysql_store_result(conn);
    if(res == nullptr) return false;
    MYSQL_ROW row;
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        likes.push_back(std::stoi(row[0]));
    }
    return true;
}

int MySQL::view(int post_id)
{
    std::string sql = R"(
    UPDATE posts
    SET view_count =
    view_count + 1
    WHERE post_id = )" + 
    std::to_string(post_id) + 
    ";";
    if(!query(sql)) return 0;
    MYSQL_RES* res = mysql_store_result(conn);
    if(res == nullptr) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    return row != nullptr?std::stoi(row[0]):0;
}

bool MySQL::checkPost(int post_id, int user_id)
{
    Statement stmt(conn, R"(
        SELECT post_id FROM
        posts
        where post_id   = ?
        and user_id     = ?;
    )");

    stmt.bindInt(0, post_id);
    stmt.bindInt(1, user_id);

    if(!stmt.execute())
    {
        return false;
    }

    stmt.storeResult();

    if(stmt.fetch())    //是当前用户的帖子
    {
        return true;
    }
    return false;
    // std::string sql = R"(
    // SELECT * FROM
    // posts
    // where post_id = )" 
    // + std::to_string(post_id) +
    // " and user_id = "
    // + std::to_string(user_id) +
    // ";";

    // if(!query(sql)) return false;
    // MYSQL_RES* res = mysql_store_result(conn);
    // if(res == nullptr) return false;
    // MYSQL_ROW row = mysql_fetch_row(res);

    // if(row == nullptr) return false;
    // return true;
}

bool MySQL::modPost(int post_id, std::string title, std::string content)
{
    Statement stmt(conn, R"(
        UPDATE posts
        SET 
        title       = ?,
        content     = ?
        where 
        post_id     = ?;
    )");
    stmt.bindString(0, title);
    stmt.bindString(1, content);
    stmt.bindInt(2, post_id);

    if(!stmt.execute()) return false;
    return true;
}

bool MySQL::load(int post_id, 
    const std::unordered_map<std::string, std::string>& fields)
{
    if (fields.empty()) {
        return true;
    }

    std::string sql;
    for (const auto& [key, value] : fields) {
        if (!sql.empty()) {
            sql += ", ";
        }
        sql += key + " = '" + value + "'";
    }

    sql = "UPDATE posts SET " + sql + " WHERE post_id = " + std::to_string(post_id);

    return query(sql);
}

bool MySQL::getPostCount(int user_id, int& count)
{
    Statement stmt(conn,R"(
        SELECT COUNT(*)
        FROM posts
        WHERE user_id=?
    )");

    stmt.bindInt(0, user_id);

    if(!stmt.execute())
    {
        std::cout<<"get postCount failrd!"<<std::endl;
        return false;
    }

    stmt.storeResult();
    stmt.fetch();
    count = stmt.row().getInt(0);

    return true;
}

bool MySQL::getCommentCount(int user_id, int& count)
{
    Statement stmt(conn,R"(
        SELECT COUNT(*)
        FROM comments
        WHERE user_id=?
    )");

    stmt.bindInt(0, user_id);

    if(!stmt.execute())
    {
        std::cout<<"get commentCount failrd!"<<std::endl;
        return false;
    }

    stmt.storeResult();
    stmt.fetch();
    count = stmt.row().getInt(0);

    return true;
}

bool MySQL::getLikeCount(int user_id, int& count)
{
    Statement stmt(conn,R"(
        SELECT COUNT(*)
        FROM post_like pl
        JOIN posts p
        ON pl.post_id=p.post_id
        WHERE p.user_id=?
    )");

    stmt.bindInt(0, user_id);

    if(!stmt.execute())
    {
        std::cout<<"get likeCount failrd!"<<std::endl;
        return false;
    }

    stmt.storeResult();
    stmt.fetch();
    count = stmt.row().getInt(0);

    return true;
}

// bool MySQL::getCreateTime(int user_id, std::string& time)
// {
//     Statement stmt(conn,R"(
//         SELECT 
//         DATE_FORMAT(create_time,'%Y-%m-%d %H:%i:%s') AS create_time
//         FROM user_info
//         WHERE user_id=?
//     )");

//     stmt.bindInt(0, user_id);

//     if(!stmt.execute())
//         return false;

//     stmt.storeResult();

//     if(!stmt.fetch())
//         return false;
    
//     time = stmt.row().getString(0);
    
//     return true;
// }

bool MySQL::getUserStat(int user_id, UserStat& stat)
{
    Statement stmt(conn,R"(
    SELECT
        DATE_FORMAT(u.create_time,'%Y-%m-%d %H:%i:%s'),

        (
            SELECT COUNT(*)
            FROM posts
            WHERE user_id=u.user_id
        ),

        (
            SELECT COUNT(*)
            FROM comments
            WHERE user_id=u.user_id
        ),

        (
            SELECT COUNT(*)
            FROM post_like pl
            JOIN posts p
            ON pl.post_id=p.post_id
            WHERE p.user_id=u.user_id
        )

    FROM user_info u
    WHERE u.user_id=?
    )");


    stmt.bindInt(0,user_id);
    if(!stmt.execute())
        return false;
    stmt.storeResult();
    if(!stmt.fetch())
        return false;

    auto row=stmt.row();

    stat.create_time=row.getString(0);
    stat.post_count=row.getInt(1);
    stat.comment_count=row.getInt(2);
    stat.like_count=row.getInt(3);

    return true;
}