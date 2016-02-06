@0xa5a99e911710891f;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("proto");

using ChannelID = UInt32;

using RelativePower = UInt16;

struct PowerState {
  channel @0 :ChannelID;
  value @1 :RelativePower;
}

struct Command {
  struct SetPower {
    channel @0 :ChannelID;
    union {
      set @1 :RelativePower;
      multiply @2 :Float32;
    }
  }

  union {
    setPower @0 :List(SetPower);
    getPower @1 :Void;
    # response: List(PowerState)

    setName @2 :Text;
    getName @3 :Void;
    # response: Text

    getChannels @4 :List(List(Float32));
    # 60 5nm buckets from 400nm to 700nm
  }
}
