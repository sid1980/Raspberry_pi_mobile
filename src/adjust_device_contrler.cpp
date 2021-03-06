#include "adjust_device_contrler.h"

device_control::device_control(){}

device_control::~device_control(){}

adjust_device_controler::adjust_device_controler(QObject *parent) : device_control(), temp_device_gpio_(0),\
    temp_device_type_(""),\
    ip_(""),\
    temp_name_("")
{
    try{

        QStringList _t_list_;
        bool        is_table_exsist = false;

        if (QSqlDatabase::contains ("Client_device_list")){
            db_ = QSqlDatabase::database ("Client_device_list");

        } else {

            db_ = QSqlDatabase::addDatabase ("QSQLITE","Client_device_list");
            db_.setDatabaseName ("Client_device_list.db");
        }

        if ( db_.open () == false){     throw Boiler_Controler_Exception(db_.lastError ().text (), __LINE__);}

        _t_list_ = db_.tables ();

        foreach (const QString & str, _t_list_){
            if ("Device_list" ==  str){   is_table_exsist = true;}
        }

        if (!is_table_exsist){

            QSqlQuery db_query(db_);
            if (db_query.exec ("CREATE TABLE `Device_list` ("
                               "`device_type`	TEXT NOT NULL,"
                               "`device_name`	TEXT NOT NULL,"
                               "`device_pid`	TEXT,"
                               "`device_hash`	TEXT,"
                               "`identify_mobile_number`	TEXT,"
                               "`device_gpio`	INTEGER,"
                               "`access_mobile_number`	INTEGER NOT NULL,"
                               "`device_active`	INTEGER NOT NULL,"
                               "PRIMARY KEY(`device_pid`)"
                               ");") == false){ throw Boiler_Controler_Exception(db_query.lastError ().text(), __LINE__);}

        }


        //안드로이드 전화번호가 필요함 일단 볼드 처리
        //owner_phone_number_ =

    }catch(Boiler_Controler_Exception& e){

        e.get_error();
    }
}

adjust_device_controler::~adjust_device_controler(){}

void adjust_device_controler::set_ip(QString ip){   ip_ = ip;}

void adjust_device_controler::set_device_name(QString name){   temp_name_ = name;}

void adjust_device_controler::set_device_type(QString type){    temp_device_type_ = type;}

void adjust_device_controler::set_device_gpio(int gpio){    temp_device_gpio_ = gpio;}

void adjust_device_controler::set_device_owner_phone_number(QString number){    owner_phone_number_ = number;}

QString adjust_device_controler::get_ip(){  return ip_;}

QString adjust_device_controler::get_device_name(){ return temp_name_;}

QString adjust_device_controler::get_device_type(){ return temp_device_type_;}

int adjust_device_controler::get_device_gpio(){ return temp_device_gpio_;}

QString adjust_device_controler::get_device_owner_phone_number(){   return owner_phone_number_;}

int adjust_device_controler::add_device()
{
    try{

        /*
         * 1. 설정된 프로퍼티를 먼저 서버에 전송
         * 2. 서버에서 리턴값이 true가 왔을경우 db에 임포트
         * 3. 만약 임포트가 안됬으면 바로 false로 리턴함
         * 4. 임포트후 프로퍼티들을 flush_device_property()호출
         *
         *
         * */



        qDebug()<<"====================[Json_Info]====================";
        qDebug()<<QString (QJsonDocument(add_device_json_form ()).toJson ());

        if (net_con.connect_raspberry (ip_, true) == false){  throw Boiler_Controler_Exception("connect_device is fail", __LINE__);}

        if (net_con.send_Object (add_device_json_form ()) == false){   throw Boiler_Controler_Exception("add_device sending add_Json_protocol fail",__LINE__);}

        QJsonObject _device_add_return_ = net_con.recv_Object ();

        net_con.disconnect_from_server ();

        if (_device_add_return_.isEmpty ()){ throw Boiler_Controler_Exception("Recv from server by add device protocol is fail", __LINE__);}

        qDebug()<<"====================[Json_Info]====================";
        qDebug()<<QString (QJsonDocument(_device_add_return_).toJson ());


        if (_device_add_return_["error"].isNull () == false){ throw Boiler_Controler_Exception(_device_add_return_["message"].toString (), __LINE__);}

        QSqlQuery db_query (db_);

        db_query.prepare ("INSERT INTO `Device_list`(`device_type`, `device_name`, `device_pid`, `device_gpio`, `access_mobile_number`, `device_active`)"
                          "VALUES (:type, :name, :pid, :gpio, :access_mobile, :device_active);");

        //임시로 pid를 0으로 설정
        db_query.bindValue (":type", temp_device_type_);
        db_query.bindValue (":name", temp_name_);
        db_query.bindValue (":pid",  _device_add_return_["pid"].toString ());
        db_query.bindValue (":gpio", temp_device_gpio_);

        //현재 전화번호를 java를 임포트해야 얻을수있으므로 중단
        //db_query.bindValue (":identify_mobile_number", owner_phone_number_);
        db_query.bindValue (":access_mobile", 0);
        db_query.bindValue (":device_active",(int)true);

        if (db_query.exec () == false){   throw Boiler_Controler_Exception(db_query.lastError ().text (), __LINE__);}

        //설정된 프로퍼티들을 전부 삭제함
        flush_device_property ();

        return _device_add_return_["pid"].toInt ();

    }catch(Boiler_Controler_Exception& e){

        //에러를 리턴함
        e.get_error();
        return -1;
    }
}

int adjust_device_controler::remove_device(int pid)
{
    try{

        QSqlQuery sql_query(db_);
        if (db_.isOpen () == false || db_.isValid () == false){

            qDebug()<<"[Debug] : db is not open ";
            return -1;
        }

        qDebug()<<"[Debug]: rcv_pid : "<<QString::number(pid);

        sql_query.prepare ("SELECT * FROM `Device_list` WHERE `device_pid` = :pid ;");
        sql_query.bindValue (":pid", QString::number(pid));

        if (sql_query.exec () == false){  throw sql_query.lastError ();}

        sql_query.last ();

        //return 2의 의미는 제거를 할수없으므로 db에 해당 pid를 존치를 한다는 의미
        if (sql_query.at () + 1 == 0){  return 2;}

        //======================= raspberry에게 접속하여 해당 pid를 삭제함 =============================

        if (net_con.connect_raspberry (ip_, true) == false){  throw Boiler_Controler_Exception("connect_device is fail", __LINE__);}

        if (net_con.send_Object (remove_device_json_form (QString::number(pid))) == false){ throw Boiler_Controler_Exception("Recv from server by remove device protocol is fail", __LINE__);}

        QJsonObject _device_add_return_ = net_con.recv_Object ();

        net_con.disconnect_from_server ();

        //만약 리턴 쿼리의 JSON이 ["Error"]가 없었거나 리턴값이 없을경우
        if ( _device_add_return_.isEmpty () ||
             ! _device_add_return_["Error"].isNull () ){ return 2;}

        //=============================== SQL에서 해당 디바이스를 삭제함 ==============================
        sql_query.prepare ("DELETE FROM `Device_list` WHERE `device_pid` = :pid ;");
        sql_query.bindValue (":pid", QString::number (pid));

        if (sql_query.exec () == false){  throw sql_query.lastError ();}

        qDebug()<<"[Info] : =================== Success delete ["<< pid <<"] ===================";

        //설정된 프로퍼티들을 전부 삭제함
        flush_device_property ();

        return 0;

    }catch(Boiler_Controler_Exception& e){
        e.get_error();
        return -1;
    }
}

int adjust_device_controler::update_device_info(int pid)
{
    try{

        if (net_con.connect_raspberry (ip_, true) == false){  throw Boiler_Controler_Exception("connect_device is fail", __LINE__);}

        if (net_con.send_Object ( update_device_json_form (QString::number (pid))) == false){   throw Boiler_Controler_Exception("add_device sending add_Json_protocol fail", __LINE__);}

        QJsonObject _device_add_return_ = net_con.recv_Object ();

        net_con.disconnect_from_server ();

        if (_device_add_return_.isEmpty ()){ throw Boiler_Controler_Exception("Recv from server by add device protocol is fail", __LINE__);}

        if (_device_add_return_["error"].isNull () == false){ throw Boiler_Controler_Exception(_device_add_return_["message"].toString (), __LINE__);}

        //설정된 프로퍼티들을 전부 삭제함
        flush_device_property ();

        return 0;

    }catch(Boiler_Controler_Exception& e){
        e.get_error();
        return -1;
    }
}

int adjust_device_controler::check_device_state()
{
    try{

        QSqlQuery db_query(db_);
        qDebug()<<"================== check_raspberry_device ==================";
        //라즈베리파이에 등록된 디바이스들의 갯수를 db에 찾아서 출력함

        /*if (db_query.exec ("SELECT COUNT(*) FROM `Device_list`;") == false){     throw db_query.lastError ().text (); }

        device_list_size = db_query.value (0).toInt ();

        db_query.clear ();*/

        if (db_query.exec ("SELECT * FROM `Device_list`;")== false){     throw Boiler_Controler_Exception(db_query.lastError ().text (), __LINE__); }

        db_query.last ();

        if (db_query.at () + 1 == 0){
            qDebug()<<"==========================================";
            qDebug()<<"NO MORE RECORD";
            qDebug()<<"==========================================";


        }else {

            db_query.first ();

            do{
                qDebug()<<"==========================================";
                qDebug()<<"[Device_Name] : "<<db_query.value(1).toString ();
                qDebug()<<"[Device_Type] : "<<db_query.value (0).toString ();
                qDebug()<<"[Device_pid ] : "<<db_query.value (2).toString ();
                qDebug()<<"[Device_hash] : "<<db_query.value (3).toString ();
                qDebug()<<"[Device_owner_number] : "<<db_query.value (4).toString ();
                qDebug()<<"\n\n\n";
            }while(db_query.next ());
        }

        db_query.clear ();

        return 0;

    }

    catch(Boiler_Controler_Exception& e){
        e.get_error();
        return -1;

    }
}

int adjust_device_controler::add_auto_upgrade_code()
{
    qDebug()<<"[Info] This will be prepared";
    return 0;
}

QJsonObject adjust_device_controler::load_device_list()
{
    /*
     * 1. 서버연결후 디바이스 목록을 요청
     * 2. 서버에서 디바이스 목록을 쿼리로 요청
     * 3. 서버에서 리턴값을 jsonObject로 변경후 리턴
     * 4. 서버에서 클라이언트로 전송
     * 5. 리턴
     *
     * */
    try{

        if (net_con.connect_raspberry (ip_, true) == false){  throw Boiler_Controler_Exception("connect_device is fail", __LINE__);}

        if (net_con.send_Object (load_device_list_json_form ()) == false){ throw Boiler_Controler_Exception("Recv from server by remove device protocol is fail", __LINE__);}

        QJsonObject _rcv_obj_ = net_con.recv_Object ();

        net_con.disconnect_from_server ();

        if (_rcv_obj_["Message"].isNull () == false){ throw _rcv_obj_["Message"].toString ();}

        return _rcv_obj_;

    }catch (QString& e){

        qDebug()<<"[Error] : " << e;

        QJsonObject res_obj;

        res_obj["Error"] = true;
        return res_obj;
    }



}

void adjust_device_controler::flush_device_property()
{
    temp_name_ = "";
    temp_device_type_ = "";
    temp_device_gpio_ = 0;
    owner_phone_number_ = "";
}

int adjust_device_controler::set_device_tempture(int pid, int tempture)
{
    /*
     * 1. 서버연결
     * 2. JsonOjbect로 프로토콜을 전송
     * 3. 서버에서 설정후 tempture를 리턴값을 받음
     * 4. 서버를 disconnect함
     * 5. tempture값을 리턴
     *
     * */

    try{

        if (net_con.connect_raspberry (ip_, true) == false){  throw Boiler_Controler_Exception("connect_device is fail", __LINE__);}

        if (net_con.send_Object (set_device_tempture_json_form (QString::number (pid), tempture)) == false){ throw Boiler_Controler_Exception("Recv from server by remove device protocol is fail", __LINE__);}

        QJsonObject _rcv_obj_ = net_con.recv_Object ();

        net_con.disconnect_from_server ();

        if (_rcv_obj_.isEmpty ()){ return 2;}

        if (_rcv_obj_["Message"].isNull () == false){ throw _rcv_obj_["Message"].toString ();}

        return _rcv_obj_["tempture"].toInt ();

    }catch (QString& e){
        qDebug()<<"[Error] : " << e;
        return -1;
    }
}

int adjust_device_controler::get_device_tempture(int pid)
{
    try{

        if (net_con.connect_raspberry (ip_, true) == false){  throw Boiler_Controler_Exception("connect_device is fail", __LINE__);}

        if (net_con.send_Object (get_device_tempture_json_form (QString::number(pid))) == false){ throw Boiler_Controler_Exception("Recv from server by remove device protocol is fail", __LINE__);}

        QJsonObject _rcv_obj_ = net_con.recv_Object ();

        net_con.disconnect_from_server ();

        if (_rcv_obj_.isEmpty ()){ return 2;}

        if (_rcv_obj_["Message"].isNull () == false){ throw _rcv_obj_["Message"].toString ();}

        return _rcv_obj_["tempture"].toInt ();

    }catch (QString& e){
        qDebug()<<"[Error] : " << e;
        return -1;
    }
}

QJsonObject adjust_device_controler::add_device_json_form()
{
    QJsonObject obj;
    obj["connect"] = true;
    obj["add_device"] = true;
    obj["d_type"] = temp_device_type_;
    obj["d_name"] = temp_name_;
    obj["d_gpio"] = temp_device_gpio_;
    obj["d_m_number"]=  owner_phone_number_;

    return obj;

}

QJsonObject adjust_device_controler::remove_device_json_form(QString d_pid)
{
    QJsonObject obj;
    obj["connect"] = true;
    obj["remove_device"] = true;
    obj["d_pid"] = d_pid;

    return obj;
}

QJsonObject adjust_device_controler::update_device_json_form(QString d_pid)
{
    QJsonObject obj;
    obj["connect"] = true;
    obj["update_device"] = true;
    obj["d_type"] = temp_device_type_;
    obj["d_name"] = temp_name_;
    obj["d_gpio"] = temp_device_gpio_;
    obj["d_pid"]  =  d_pid;

    return obj;
}

QJsonObject adjust_device_controler::set_device_tempture_json_form(QString d_pid, int tempture)
{
    QJsonObject obj;

    obj["connect"] = true;
    obj["d_pid"]   = d_pid;
    obj["tempture"] = tempture;
    obj["adjust"]  = "set";

    return obj;
}

QJsonObject adjust_device_controler::get_device_tempture_json_form(QString d_pid)
{
    QJsonObject obj;

    obj["connect"] = true;
    obj["d_pid"] = d_pid;
    obj["tempture"] = 0;
    obj["adjust"] = "get";

    return obj;

}

QJsonObject adjust_device_controler::load_device_list_json_form()
{
    QJsonObject obj;

    obj["connect"] = true;
    obj["load_device_info"] = true;

    return obj;
}
