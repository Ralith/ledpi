# Example configuration for an RGBW light
# Serialize as follows:
# $ capnp eval -p rgbw.capnp state > /var/lib/ledpi-state

@0xa71c79b0a02cf0d0;

using import "state.capnp".State;

const state :State =
( channels =
  [ (name = "red", gpio = 0, spectra = [])
  , (name = "green", gpio = 1, spectra = [])
  , (name = "blue", gpio = 4, spectra = [])
  , (name = "white", gpio = 17, spectra = [])
  ]
, name = "RGBW lamp"
, defaultLevels = (previous = void)
, levels = [0, 0, 0, 0]
);
