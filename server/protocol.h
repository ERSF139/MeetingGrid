#pragma once

// 信令协议常量：与 docs/信令协议.md 保持一致。
// 消息统一为 JSON 文本帧：{"type":"...", ...}
namespace Protocol {

// ---- 客户端 → 服务端 ----
namespace C2S {
inline constexpr const char *CreateRoom = "create_room";  // {name}
inline constexpr const char *JoinRoom   = "join_room";    // {room_id, name}
inline constexpr const char *LeaveRoom  = "leave_room";   // {}
inline constexpr const char *SetState   = "set_state";    // {mic?, cam?} 只传变化字段
inline constexpr const char *SetShare   = "set_share";    // {on}
inline constexpr const char *Chat       = "chat";         // {text}
inline constexpr const char *Ping       = "ping";         // {}
} // namespace C2S

// ---- 服务端 → 客户端 ----
namespace S2C {
inline constexpr const char *Hello         = "hello";          // {version}
inline constexpr const char *RoomCreated   = "room_created";   // {room_id}
inline constexpr const char *Joined        = "joined";         // {room_id, member_id, members[]}
inline constexpr const char *MemberJoined  = "member_joined";  // {member}
inline constexpr const char *MemberLeft    = "member_left";    // {member_id}
inline constexpr const char *MemberUpdated = "member_updated"; // {member_id, mic, cam, sharing}
inline constexpr const char *Chat          = "chat";           // {member_id, name, text}
inline constexpr const char *RoomClosed    = "room_closed";    // {}
inline constexpr const char *Error         = "error";          // {code, message}
inline constexpr const char *Pong          = "pong";           // {}
} // namespace S2C

// ---- JSON 字段名 ----
inline constexpr const char *KeyType     = "type";
inline constexpr const char *KeyId       = "id";         // member 对象内的成员 id
inline constexpr const char *KeyName     = "name";
inline constexpr const char *KeyRoomId   = "room_id";
inline constexpr const char *KeyMemberId = "member_id";
inline constexpr const char *KeyMembers  = "members";
inline constexpr const char *KeyMember   = "member";
inline constexpr const char *KeyMic      = "mic";
inline constexpr const char *KeyCam      = "cam";
inline constexpr const char *KeySharing  = "sharing";
inline constexpr const char *KeyOn       = "on";
inline constexpr const char *KeyText     = "text";
inline constexpr const char *KeyCode     = "code";
inline constexpr const char *KeyMessage  = "message";
inline constexpr const char *KeyVersion  = "version";
inline constexpr const char *KeyIsHost   = "isHost";

// ---- 媒体帧（二进制信封 header） ----
inline constexpr const char *KeyKind = "kind";   // video | audio | screen
inline constexpr const char *KeyW    = "w";
inline constexpr const char *KeyH    = "h";

// 媒体帧类型
namespace MediaKind {
inline constexpr const char *Video = "video";
inline constexpr const char *Audio = "audio";
inline constexpr const char *Screen = "screen";
} // namespace MediaKind

// ---- 错误码 ----
inline constexpr const char *ErrInRoom        = "already_in_room";
inline constexpr const char *ErrRoomNotFound  = "room_not_found";
inline constexpr const char *ErrRoomFull      = "room_full";
inline constexpr const char *ErrBadRequest    = "bad_request";

// ---- 房间限制 ----
inline constexpr int MaxRoomMembers = 49; // 对齐客户端滑块上限
inline constexpr int RoomIdDigits   = 9;

// ---- 服务端版本 ----
inline constexpr const char *ServerVersion = "0.1.0";

} // namespace Protocol
