@0xa5a99e911710891f;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("proto");

using import "common.capnp".Channel;

using ChannelID = UInt32;

using RelativePower = UInt16;

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

    setName @2 :Text;
    getName @3 :Void;

    getChannels @4 :Void;
  }
}

struct Response {
  struct Power {
    channel @0 :ChannelID;
    value @1 :RelativePower;
  }

  union {
    power @0 :List(Power);
    name @1 :Text;
    channels @2 :List(Channel);
  }
}
