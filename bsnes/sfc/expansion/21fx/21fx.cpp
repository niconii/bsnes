#include <sfc/sfc.hpp>

namespace SuperFamicom {

S21FX::S21FX() {
  create(S21FX::Enter, 10'000'000);

  resetVector = bus.read(0xfffc, 0x00);
  resetVector |= bus.read(0xfffd, 0x00) << 8;

  bus.map({&S21FX::read, this}, {&S21FX::write, this}, "00-3f,80-bf:2184-21ff");
  bus.map({&S21FX::read, this}, {&S21FX::write, this}, "00:fffc-fffd");

  booted = false;

  for(auto& byte : ram) byte = 0xdb;  //stp
  ram[0] = 0x6c;  //jmp ($fffc)
  ram[1] = 0xfc;
  ram[2] = 0xff;

  if(auto buffer = file::read("21fx.rom")) {
    memory::copy(ram, sizeof(ram), buffer.data(), buffer.size());
  }

  if(link.openAbsolute("./21fx.so")) {
    linkInit = link.sym("fx_init");
    linkMain = link.sym("fx_main");
    linkQuit = link.sym("fx_quit");
    if(linkInit) linkInit({&S21FX::step, this}, snesBuffer, linkBuffer);
  }
}

S21FX::~S21FX() {
  bus.unmap("00-3f,80-bf:2184-21ff");
  bus.unmap("00:fffc-fffd");

  //note: this is an awful hack ...
  //since the bus maps are lambdas, we can't safely restore the original reset vector handler
  //as such, we install a basic read-only lambda that simply returns the known reset vector
  //the downside is that if 00:fffc-fffd were anything but ROM; it will now only act as ROM
  //given that this is the only device that hooks the reset vector like this,
  //it's not worth the added complexity to support some form of reversible bus mapping hooks
  uint vector = resetVector;
  bus.map([vector](uint24 addr, uint8) -> uint8 {
    return vector >> addr * 8;
  }, [](uint24, uint8) -> void {
  }, "00:fffc-fffd", 2);

  if(linkQuit) linkQuit();
  if(link.open()) link.close();
  linkInit.reset();
  linkMain.reset();
  linkQuit.reset();
}

auto S21FX::Enter() -> void {
  while(true) {
    scheduler.synchronize();
    expansionPort.device->main();
  }
}

auto S21FX::synchronizeCPU() -> void {
  if(clock >= 0) scheduler.resume(cpu.thread);
}

auto S21FX::step(uint clocks) -> void {
  clock += clocks * (uint64_t)cpu.frequency;
  synchronizeCPU();
}

auto S21FX::main() -> void {
  if(linkMain) linkMain();
  while(true) step(10'000'000);
}

auto S21FX::read(uint addr, uint8 data) -> uint8 {
  addr &= 0x40ffff;

  if(addr == 0xfffc) return booted ? resetVector & 0xff : (uint8)0x84;
  if(addr == 0xfffd) return booted ? resetVector >> 8   : (booted = true, (uint8)0x21);

  if(addr >= 0x2184 && addr <= 0x21fd) return ram[addr - 0x2184];

  if(addr == 0x21fe) return !link.open() ? 0 : (
    (linkBuffer.size() >    0) << 7  //1 = readable
  | (snesBuffer.size() < 1024) << 6  //1 = writable
  | (link.open())              << 5  //1 = connected
  );

  if(addr == 0x21ff) {
    if(linkBuffer.size() > 0) {
      return linkBuffer.takeLeft();
    }
  }

  return data;
}

auto S21FX::write(uint addr, uint8 data) -> void {
  addr &= 0x40ffff;

  if(addr == 0x21ff) {
    if(snesBuffer.size() < 1024) {
      snesBuffer.append(data);
    }
  }
}

}
