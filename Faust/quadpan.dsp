import("stdfaust.lib");

make_channel(i) = sp.spat(4, pan, level)
with {
    j = i + 8;
    pan = hslider("Pan%i[OWL:%i]", i / 4, 0.0, 1.0, 0.001);
    level = hslider("Level%i[OWL:%j]", 1.0, 0.0, 1.0, 0.001);
};

panners = par(
    i,
    4,
    make_channel(i)
);

process = panners :> _, _, _, _;
