// MeetingGrid 信令服务端 冒烟测试
// 运行前提：Node >= 21（使用内置全局 WebSocket，无需 npm install）
// 用法：
//   build\MeetingGridServer.exe            # 先启动服务端（默认 8090）
//   node server/test/smoke_test.mjs [port] # 再运行本脚本
//
// 覆盖流程：建会 → 入会(含错误路径) → 状态同步 → 共享 → 聊天 → ping → 退会
//          → 主持人离开关房。全部断言通过则退出码 0。

import process from 'node:process';

const PORT = Number(process.argv[2] ?? 8090);
const URL = `ws://127.0.0.1:${PORT}`;

let passed = 0;
let failed = 0;

function check(cond, label) {
  if (cond) {
    passed++;
    console.log(`  PASS  ${label}`);
  } else {
    failed++;
    console.error(`  FAIL  ${label}`);
  }
}

function connect() {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(URL);
    const inbox = [];
    const waiters = [];
    ws.addEventListener('open', () => resolve(ws));
    ws.addEventListener('error', () => reject(new Error(`无法连接 ${URL}`)));
    ws.addEventListener('message', (ev) => {
      let msg;
      try {
        msg = JSON.parse(ev.data);
      } catch {
        return;
      }
      const i = waiters.findIndex((w) => w.type === msg.type);
      if (i >= 0) {
        const [w] = waiters.splice(i, 1);
        clearTimeout(w.timer);
        w.resolve(msg);
      } else {
        inbox.push(msg);
      }
    });
    ws.sendJson = (obj) => ws.send(JSON.stringify(obj));
    ws.waitFor = (type, timeout = 4000) => {
      const i = inbox.findIndex((m) => m.type === type);
      if (i >= 0) return Promise.resolve(inbox.splice(i, 1)[0]);
      return new Promise((resolveMsg, rejectMsg) => {
        const timer = setTimeout(() => rejectMsg(new Error(`等待 ${type} 超时`)), timeout);
        waiters.push({ type, resolve: resolveMsg, timer });
      });
    };
  });
}

async function main() {
  console.log(`连接 ${URL} ...`);
  const A = await connect();
  const B = await connect();
  const C = await connect();

  // 1. hello
  await A.waitFor('hello');
  await B.waitFor('hello');
  await C.waitFor('hello');
  console.log('三客户端已连接');

  // 2. A 创建房间
  A.sendJson({ type: 'create_room', name: '主持人' });
  const created = await A.waitFor('room_created');
  const joinedA = await A.waitFor('joined');
  const roomId = created.room_id;
  check(/^\d{9}$/.test(roomId), `房间号是 9 位数字（${roomId}）`);
  check(joinedA.members.length === 1, '创建后成员列表为 1 人');
  check(joinedA.members[0].id === joinedA.member_id, 'joined 的 member_id 与成员一致');
  check(joinedA.members[0].isHost === true, '创建者为主持人');

  // 3. 错误路径：加入不存在的房间
  B.sendJson({ type: 'join_room', room_id: '000000001', name: '路人' });
  const err = await B.waitFor('error');
  check(err.code === 'room_not_found', `加入不存在房间收到错误（${err.code}）`);

  // 4. B 加入房间
  B.sendJson({ type: 'join_room', room_id: roomId, name: '张伟' });
  const joinedB = await B.waitFor('joined');
  const mj = await A.waitFor('member_joined');
  check(joinedB.members.length === 2, 'B 入会后成员列表为 2 人');
  check(mj.member.name === '张伟' && mj.member.isHost === false, 'A 收到 B 加入广播');
  check(joinedB.member_id === mj.member.id, '双方对 B 的 member_id 一致');
  const bid = mj.member.id;

  // 5. 状态同步：A 关麦克风
  A.sendJson({ type: 'set_state', mic: false });
  const updA = await A.waitFor('member_updated');
  const updB = await B.waitFor('member_updated');
  check(updA.mic === false && updA.cam === true, 'A 麦克风关闭广播（mic=false, cam=true）');
  check(updB.member_id === updA.member_id, 'B 也收到同一成员的状态广播');

  // 6. 共享
  B.sendJson({ type: 'set_share', on: true });
  const shA = await A.waitFor('member_updated');
  check(shA.member_id === bid && shA.sharing === true, 'B 开始共享屏幕广播');

  // 7. 聊天
  B.sendJson({ type: 'chat', text: '大家好' });
  const chatB = await B.waitFor('chat');
  const chatA = await A.waitFor('chat');
  check(chatA.text === '大家好' && chatA.name === '张伟', '聊天消息广播（含发送者名字）');
  check(chatB.member_id === bid, '发送者本人也收到自己的聊天（可用于回显）');

  // 8. ping / pong
  B.sendJson({ type: 'ping' });
  await B.waitFor('pong');
  check(true, 'ping → pong');

  // 9. B 主动离开
  B.sendJson({ type: 'leave_room' });
  const leftA = await A.waitFor('member_left');
  check(leftA.member_id === bid, 'B 离开后 A 收到 member_left');
  B.close();

  // 10. C 加入 A 的房间（此时房间只剩主持人 A）
  C.sendJson({ type: 'join_room', room_id: roomId, name: '李娜' });
  await C.waitFor('joined');
  await A.waitFor('member_joined');
  check(true, 'C 加入 A 的房间');

  // 11. 主持人 A 离开 → 房间关闭，C 收到 room_closed
  A.close();
  const closedC = await C.waitFor('room_closed');
  check(closedC.type === 'room_closed', '主持人离开后 C 收到 room_closed');
  C.close();

  console.log(`\n结果: ${passed} passed, ${failed} failed`);
  process.exitCode = failed === 0 ? 0 : 1;
}

main().catch((e) => {
  console.error(`\n测试异常: ${e.message}`);
  process.exitCode = 1;
});
