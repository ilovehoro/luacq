/*
* Luacq - Lua interface enabled cq plugin
* Api Version 9
* Written by imkzh based on cq-vcsdk
*/
#include "stdafx.h"
#include "string"
#include "cqp.h"
#include "appmain.h" //应用AppID等信息，请正确填写，否则酷Q可能无法加载

#ifdef __cplusplus
	#include "lua_src\lua.hpp"
#else
	#include "lua_src\lua.h"
	#include "lua_src\lualib.h"
	#include "lua_src\lauxlib.h"
#endif

#include "luawrappers.h"
#include "Events.h"
#include "DebugLogging.h"

#include "iconv.h"

using namespace std;

extern char InterfaceName[50];
extern unsigned char IsInterfaceNameSet;


int ac = -1; //AuthCode 调用酷Q的方法时需要用到
bool enabled = false;
lua_State * state;

/*
	Event Startup();
	Event Exit();
	Event Enable();
	Event Disable();
	Event PrivateMsg(msgType, senderID, sendTime, Msg, Font);
	Event GroupMsg(msgType, groupID, senderID, Anonymous, sendTime, Msg, Font);
	Event DiscussMsg(msgType, discussID, senderID, sendTime, Msg, Font);
	Event GroupAdminSet(operationType, groupID, infectedID, sendTime);
	Event GroupMemberDecreased(infoType, groupID, sendTime, infectedID, operatorID);
	Event GroupMemberIncreased(infoType, groupID, sendTime, infectedID, operatorID);
	Event FriendAdded(infoType, sendTime, infectedID);
	Event FriendAddRequesting(requestType, sendTime, senderID, Msg) returns RESPONSE;
	Event GroupAddRequesting(requestType, sendTime, senderID, Msg) returns RESPONSE;
*/

/* 
* 返回应用的ApiVer、Appid，打包后将不会调用
*/
CQEVENT(const char*, AppInfo, 0)() {
	return CQAPPINFO;
}


/* 
* 接收应用AuthCode，酷Q读取应用信息后，如果接受该应用，将会调用这个函数并传递AuthCode。
* 不要在本函数处理其他任何代码，以免发生异常情况。如需执行初始化代码请在Startup事件中执行（Type=1001）。
*/
CQEVENT(int32_t, Initialize, 4)(int32_t AuthCode) {
	ac = AuthCode;
	return 0;
}


/*
*
* Type=1001 酷Q启动
* 无论本应用是否被启用，本函数都会在酷Q启动后执行一次，请在这里执行应用初始化代码。
* 如非必要，不建议在这里加载窗口。（可以添加菜单，让用户手动打开窗口）
*/
CQEVENT(int32_t, __eventStartup, 0)() {
	
	Debug_Start();
	Debug_Write("lua_doInit().\n");
	state = lua_doInit();
	Debug_Write("Lua Loaded!\n");
	return 0;
}

/*
* Type=1002 酷Q退出
* 无论本应用是否被启用，本函数都会在酷Q退出前执行一次，请在这里执行插件关闭代码。
* 本函数调用完毕后，酷Q将很快关闭，请不要再通过线程等方式执行其他代码。
*/
CQEVENT(int32_t, __eventExit, 0)() {
	lua_close(state);
	Debug_Stop();
	return 0;
}

/*
* Type=1003 应用已被启用
* 当应用被启用后，将收到此事件。
* 如果酷Q载入时应用已被启用，则在_eventStartup(Type=1001,酷Q启动)被调用后，本函数也将被调用一次。
* 如非必要，不建议在这里加载窗口。（可以添加菜单，让用户手动打开窗口）
*/
CQEVENT(int32_t, __eventEnable, 0)() {
	enabled = true;
	return 0;
}


/*
* Type=1004 应用将被停用
* 当应用被停用前，将收到此事件。
* 如果酷Q载入时应用已被停用，则本函数*不会*被调用。
* 无论本应用是否被启用，酷Q关闭前本函数都*不会*被调用。
*/
CQEVENT(int32_t, __eventDisable, 0)() {
	enabled = false;
	return 0;
}
  
/* 
* Type=21 私聊消息
* subType 子类型，11/来自好友 1/来自在线状态 2/来自群 3/来自讨论组
*/
CQEVENT(int32_t, __eventPrivateMsg, 24)(int32_t subType, int32_t sendTime, int64_t fromQQ, const char *msg, int32_t font) {

	//如果要回复消息，请调用酷Q方法发送，并且这里 return EVENT_BLOCK - 截断本条消息，不再继续处理  注意：应用优先级设置为"最高"(10000)时，不得使用本返回值
	//如果不回复消息，交由之后的应用/过滤器处理，这里 return EVENT_IGNORE - 忽略本条消息
	Debug_Write("Event PrivateMSG\n");

	char * message;
	gbk2utf(msg, &message);


	if (IsInterfaceNameSet && enabled) {
		//msgType, senderID, sendTime, Msg, Font
		//Debug_Write("  forking new thread..\n");
		//char s[512];
		//sprintf_s(s, 512, "state pointed to %ld", state);
		//Debug_Write(s);
		//lua_State * tstate = lua_newthread(state);

		Debug_Write("  Pushing Parameters..\n");
		lua_getglobal(state, InterfaceName);
		lua_getfield(state, -1, "PostMessage");
		lua_pushinteger(state, EVENT_PrivateMsg);
		lua_pushinteger(state, subType);
		lua_pushinteger(state, fromQQ);
		lua_pushinteger(state, sendTime);
		lua_pushstring(state, message);
		lua_pushinteger(state, font);
		Debug_Write("  [c->lua|CQAPI.PostMessage]Do Lua_call()..\n");
		int result = lua_pcall(state, 6, 1, 0);

		if (result) {
			Debug_Write("lua_pcall_err\n");
			lua_throw(state);
		} else {
			Debug_Write("  Call returned without error\n");
		}
		 
		Debug_Write("  Getting lua script Return..\n");
		result = (int)lua_tointeger(state, -1);

		char s[30];
		sprintf_s(s, 30, "result = %d\n", result);
		Debug_Write(s);

		free(message);
		return result;
	}
	else{
		if (!enabled) {
			Debug_Write("Plugin Not Enabled!");
		}
		if(!IsInterfaceNameSet) {
			Debug_Write("Plugin Interface Name Not Set!");
		}
	}
	free(message);
	Debug_Write("PrivateMSG Discarded\n");
	//return EVENT_BLOCK;
	return EVENT_IGNORE;
}


/*
* Type=2 群消息
*/
CQEVENT(int32_t, __eventGroupMsg, 36)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, const char *fromAnonymous, const char *msg, int32_t font) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=4 讨论组消息
*/
CQEVENT(int32_t, __eventDiscussMsg, 32)(int32_t subType, int32_t sendTime, int64_t fromDiscuss, int64_t fromQQ, const char *msg, int32_t font) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=101 群事件-管理员变动
* subType 子类型，1/被取消管理员 2/被设置管理员
*/
CQEVENT(int32_t, __eventSystem_GroupAdmin, 24)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=102 群事件-群成员减少
* subType 子类型，1/群员离开 2/群员被踢 3/自己(即登录号)被踢
* fromQQ 操作者QQ(仅subType为2、3时存在)
* beingOperateQQ 被操作QQ
*/
CQEVENT(int32_t, __eventSystem_GroupMemberDecrease, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=103 群事件-群成员增加
* subType 子类型，1/管理员已同意 2/管理员邀请
* fromQQ 操作者QQ(即管理员QQ)
* beingOperateQQ 被操作QQ(即加群的QQ)
*/
CQEVENT(int32_t, __eventSystem_GroupMemberIncrease, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, int64_t beingOperateQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=201 好友事件-好友已添加
*/
CQEVENT(int32_t, __eventFriend_Add, 16)(int32_t subType, int32_t sendTime, int64_t fromQQ) {

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=301 请求-好友添加
* msg 附言
* responseFlag 反馈标识(处理请求用)
*/
CQEVENT(int32_t, __eventRequest_AddFriend, 24)(int32_t subType, int32_t sendTime, int64_t fromQQ, const char *msg, const char *responseFlag) {

	//CQ_setFriendAddRequest(ac, responseFlag, REQUEST_ALLOW, "");

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}


/*
* Type=302 请求-群添加
* subType 子类型，1/他人申请入群 2/自己(即登录号)受邀入群
* msg 附言
* responseFlag 反馈标识(处理请求用)
*/
CQEVENT(int32_t, __eventRequest_AddGroup, 32)(int32_t subType, int32_t sendTime, int64_t fromGroup, int64_t fromQQ, const char *msg, const char *responseFlag) {

	//if (subType == 1) {
	//	CQ_setGroupAddRequestV2(ac, responseFlag, REQUEST_GROUPADD, REQUEST_ALLOW, "");
	//} else if (subType == 2) {
	//	CQ_setGroupAddRequestV2(ac, responseFlag, REQUEST_GROUPINVITE, REQUEST_ALLOW, "");
	//}

	return EVENT_IGNORE; //关于返回值说明, 见“_eventPrivateMsg”函数
}

/*
* 菜单，可在 .json 文件中设置菜单数目、函数名
* 如果不使用菜单，请在 .json 及此处删除无用菜单
*/
CQEVENT(int32_t, __menuA, 0)() {
	MessageBoxA(NULL, "这是menuA，在这里载入窗口，或者进行其他工作。", "" ,0);
	return 0;
}

CQEVENT(int32_t, __menuB, 0)() {
	MessageBoxA(NULL, "这是menuB，在这里载入窗口，或者进行其他工作。", "" ,0);
	return 0;
}
