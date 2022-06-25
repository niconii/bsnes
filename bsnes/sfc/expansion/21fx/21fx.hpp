struct S21FX : Expansion {
  S21FX();
  ~S21FX();

  auto synchronizeCPU() -> void;
  static auto Enter() -> void;
  auto step(uint clocks) -> void;
  auto main() -> void;

  auto read(uint addr, uint8 data) -> uint8;
  auto write(uint addr, uint8 data) -> void;

private:
  bool booted = false;
  uint16 resetVector;
  uint8 ram[122];

  nall::library link;
  function<void (
    function<void (uint)>,  //step
    vector<uint8>&,         //snesBuffer
    vector<uint8>&          //linkBuffer
  )> linkInit;
  function<void ()> linkMain;
  function<void ()> linkQuit;

  vector<uint8> snesBuffer;  //SNES -> Link
  vector<uint8> linkBuffer;  //Link -> SNES
};
