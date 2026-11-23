// DON'T LOOK — copy, items, and tuning.
// The house layout lives in world.mjs. Photos are wired in photos.mjs.
//
// chris.jpg       player
// mom.jpg         in the car; the voice in the house is not her
// her.jpg         turquoise shirt; she had the car keys
// evan.jpg        the hat. The trail photo Ben opened. Not Ben.
// teammate.jpg    gear bag in the garage. Looked anyway.
// basement.jpg    scratched photo by the tanks
// grin.jpg        the picture. The only face the thing wears.
// Ben sends the texts. His face is not one of these photos.
//
// "Her" is unnamed on purpose. Rename ROOMS.her and these lines if you want one.

export const TUNING = {
  slots: 6,
  walk: 96,
  sneak: 50,
  run: 156,
  dodge: 300,
  dodgeTime: 0.16,
  dodgeCost: 42,
  runDrain: 22,
  stamRegen: 16,
  lightDrain: 0.7,
  lightRange: 188,
  cone: 0.82,
  feet: 72,
  feetDark: 40,
  interact: 44,
  holdTime: 1.35,
  meleeIFrames: 0.65,
  enemyCool: 0.9,
};

export const ITEMS = {
  stick: {
    name: 'Hockey Stick',
    type: 'weapon',
    dmg: 18,
    dur: 18,
    range: 38,
    arc: 1.2,
    cool: 0.46,
    noise: 34,
    desc: "Taped for Thursday's game. Quiet, until the wrap gives. The roll of tape is in your room, by the bed.",
  },
  knife: {
    name: 'Kitchen Knife',
    type: 'weapon',
    dmg: 26,
    dur: 7,
    range: 28,
    arc: 0.85,
    cool: 0.32,
    noise: 22,
    desc: 'From beside the kitchen counter. Short, sharp, and it will not last.',
  },
  pistol: {
    name: 'Revolver',
    type: 'weapon',
    dmg: 40,
    range: 210,
    arc: 0.18,
    cool: 0.62,
    noise: 340,
    ammo: true,
    desc: 'Loud enough to call every room. There is no street outside to hear it.',
  },
  ammo: {
    name: 'Rounds',
    type: 'ammo',
    stack: 12,
    desc: 'Not many. Make the one you spend count.',
  },
  bandage: {
    name: 'Bandage',
    type: 'heal',
    heal: 34,
    stack: 4,
    desc: 'Enough to keep walking.',
  },
  pills: {
    name: 'Painkillers',
    type: 'heal',
    heal: 78,
    stack: 2,
    desc: 'The date on the bottle is still good. Nothing else is.',
  },
  film: {
    name: 'Film Slide',
    type: 'film',
    stack: 3,
    desc: 'One exposure. The garage bench can hold a room in place. There are not many slides.',
  },
  tape: {
    name: 'Stick Tape',
    type: 'tape',
    desc: 'For the hockey stick, before the shaft gives out.',
  },
  'key-her': {
    name: 'Her Key',
    type: 'key',
    desc: 'Brass, from the cabinet on the left side of the bathroom. It fits her door on the upper hall.',
  },
  'key-base': {
    name: 'Basement Key',
    type: 'key',
    desc: "Warm from the sweater in Mom's room. The basement door is in the mudroom.",
  },
  'key-shed': {
    name: 'Shed Key',
    type: 'key',
    desc: 'A ring with the name scratched off. The shed is in the yard.',
  },
  'key-car': {
    name: 'Car Keys',
    type: 'key',
    desc: 'Yours. She had them again. They go in the ignition.',
  },
  fuse: {
    name: 'Fuse',
    type: 'quest',
    desc: 'The yard door in the garage is dead without this. Seat it in the car.',
  },
  gas: {
    name: 'Gas Can',
    type: 'quest',
    desc: 'Heavy. The gauge was on E. So is the rest of the night.',
  },
  camera: {
    name: "Ben's Camera",
    type: 'camera',
    flashes: 3,
    desc: 'The bulbs stun whatever is close. The plate in the back is for the car, not the face.',
  },
};

export const ENEMY = {
  shambler: { hp: 56, speed: 62, dmg: 12, reach: 16, radius: 10 },
  grin: { hp: 96, speed: 118, dmg: 26, reach: 20, radius: 12, revive: 16 },
};

export const MOM_LINES = [
  'Chris? Come in, honey. The door is open.',
  "I'm in my room. Leave the light off.",
  'I kept a plate warm for you.',
  "Come closer. I can't see you from here.",
  "You looked at it, didn't you.",
  'Ben said you would.',
  'Come to my room.',
  'I have the blue sweater on. Come see.',
];

// After the sweater key. Same voice, wrong details, still readable.
export const MOM_LINES_LATE = [
  'Chris? Chris. The front door is open.',
  "I'm in your room. Leave the light off.",
  'I kept your plate. It is still warm.',
  'Come closer. I can see you from here.',
  'You looked. You looked at it.',
  'Ben said you already did.',
  'Come down the hall. Come down the hall.',
  "I'm wearing the blue sweater. Come see.",
];

export const NOTES = {
  foyer: {
    title: "Mom's note",
    body: "Chris —\n\nShe has your keys again. The flashlight is a few steps from this note, still in the foyer.\n\nThe basement key is in my room, in the sweater pocket. If you can already hear me, that isn't me. I'm in the car. Don't answer.\n\nYour hands hold six things. The bin in the garage keeps the rest.",
  },
  kitchen: {
    title: 'Grocery list',
    body: 'milk\nbatteries\nthe good bread\n\nleft side of the bathroom. her key is in the cabinet. the lock sticks.',
  },
  hockey: {
    title: 'Thursday',
    body: "Thursday. We won.\n\nThe film slide is with the two rounds, on this side of the room. The tape is by the bed.\n\nIf the car won't leave, the garage bench takes one slide. There aren't many.",
  },
  journal: {
    title: 'Her journal',
    body: "Mom said to stay in the car. The gauge was on E, so I went for the can. It's in the shed, by the shelf. The yard door wouldn't lift. No power.\n\nI heard Ben in the basement, in the tanks. It wasn't his voice for long.\n\nIf you're reading this, get the gas. Don't look into the tanks.",
  },
  diary: {
    title: "Mom's diary",
    body: "Ben called before midnight. He was crying. He said the picture from the trail had opened, and that he sent it to you anyway.\n\nI'm going to sit in the car with the doors locked. If the house says my name, it learned it from the photo.\n\nThe basement key is in the sweater. Don't take it if the bed is breathing.",
  },
  ray: {
    title: 'Scratched note',
    body: "I pulled the fuse so the yard door couldn't open. It wasn't going to see the grass.\n\nThe fuse is by the bench. The camera too. The bulbs stun it. The plate in the back is only for the car. Photograph the car, not the face.\n\nThe shed key is the ring. The name on it is gone.",
  },
  bag: {
    title: 'On the lock screen',
    photo: 'teammate',
    body: "you left this in my car.\n\ndon't open the picture ben sent.\n\ni looked.\ni'm fine.\ni'm fine.",
  },
};

export const INSPECTIONS = {
  duffel: {
    title: 'Your bag',
    body: "The stick isn't in it. You left it in the living room, left of the couch. The rest of the gear is in the garage.",
  },
  front: {
    title: 'Front door',
    body: 'The door is here. The street is not. The wood is warm, like breath on the other side.',
  },
  family: {
    title: 'The yard, before',
    body: "Chris, her, and Mom. An afternoon that still looks normal. If Mom's face starts to change, put it down.",
    photos: [
      { id: 'chris', crop: 'bust', label: 'Chris' },
      { id: 'her', crop: 'bust', label: 'Her' },
      { id: 'mom', crop: 'bust', label: 'Mom' },
    ],
  },
  tv: {
    title: 'Television',
    body: 'Static. For a second the smile is in the glass, closer than the screen.',
    photos: [{ id: 'grin', crop: 'face', label: '' }],
  },
  herpic: {
    title: 'Her photo',
    body: "She's laughing. The shirt is turquoise. Nothing else in the house is that color now.",
    photos: [{ id: 'her', crop: 'bust', label: '' }],
  },
  mirror: {
    title: 'Mirror',
    body: "THAT IS NOT MOM\n\nLipstick, pressed until the tube tore. The mouth in the glass isn't yours.",
    photos: [{ id: 'chris', crop: 'face', label: 'You', grin: 0.45, corrupt: 0.7 }],
  },
  mompic: {
    title: "Mom's photo",
    body: "Mom, in the yard. The blue sweater in the picture is the one on the bed. She isn't in it.",
    photos: [{ id: 'mom', crop: 'bust', label: 'Mom' }],
  },
  badge: {
    title: 'A scratched photo',
    body: 'The name is scratched out. The tanks behind him are the ones still running.',
    photos: [{ id: 'basement', crop: 'bust', label: '' }],
  },
  tanks: {
    title: 'The tanks',
    body: "They haven't stopped humming. The glass wants to show the other face.",
    photos: [
      { id: 'basement', crop: 'face', label: 'Then' },
      { id: 'grin', crop: 'face', label: 'Now' },
    ],
  },
  yard: {
    title: 'The yard',
    body: 'The grass is the right shape and the wrong color. Where the street should be, there is only the dark.',
  },
  cloth: {
    title: 'Torn cloth',
    body: "Turquoise, caught by the shelf. The same shirt as the photo. She isn't here.",
    photos: [{ id: 'her', crop: 'face', label: '' }],
  },
};

export function describeGoal(c) {
  const I = c.installed;
  const R = c.read || {};
  const L = c.loc || {};
  const at = (id) => L[id] || 'world';
  const rules = [
    [c.finale, 'Hold at the car. Do not let it in.'],
    [c.room === 'her' && !I.keys && at('key-car') === 'world', 'The car keys are beside her journal.'],
    [c.room === 'shed' && !I.gas && at('gas') === 'world', 'The gas can is by the shelf.'],
    [c.room === 'basement' && !I.fuse && at('fuse') === 'world', 'By the bench: fuse, camera, shed key. A gun here is loud.'],

    [I.fuse && I.keys && I.gas && !I.photo && at('camera') === 'inv', 'At the car, photograph it. Not the face.'],
    [I.fuse && I.keys && I.gas && !I.photo && at('camera') === 'stash', "Ben's camera is in the garage bin."],
    [I.fuse && I.keys && I.gas && !I.photo && at('camera') === 'floor', "Ben's camera is where you set it down."],
    [I.fuse && I.keys && I.gas && !I.photo, "Ben's camera is by the basement bench."],

    [!I.fuse && at('fuse') === 'inv', 'Seat the fuse in the car. It opens the yard.'],
    [!I.fuse && at('fuse') === 'stash', 'The fuse is in the garage bin.'],
    [!I.fuse && at('fuse') === 'floor', 'The fuse is where you set it down.'],

    [!I.gas && at('gas') === 'inv', 'Pour the gas can into the car.'],
    [!I.gas && at('gas') === 'stash', 'The gas can is in the garage bin.'],
    [!I.gas && at('gas') === 'floor', 'The gas can is where you set it down.'],

    [!I.keys && at('key-car') === 'inv', 'The car keys go in the ignition.'],
    [!I.keys && at('key-car') === 'stash', 'The car keys are in the garage bin.'],
    [!I.keys && at('key-car') === 'floor', 'The car keys are where you set them down.'],

    [I.fuse && !I.gas && at('key-shed') === 'stash', 'The shed key is in the garage bin.'],
    [I.fuse && !I.gas && at('key-shed') === 'floor', 'The shed key is where you set it down.'],
    [I.fuse && !I.gas && (R.journal || R.ray || at('key-shed') === 'inv' || c.shedOpen), 'The shed is in the yard. The can is by the shelf.'],
    [I.fuse && !I.gas, 'The yard door is open. Find the fuel.'],

    [!I.fuse && R.ray && at('fuse') === 'world', 'By the basement bench: fuse, camera, shed key.'],
    [!I.fuse && at('key-base') === 'stash', 'The basement key is in the garage bin.'],
    [!I.fuse && at('key-base') === 'floor', 'The basement key is where you set it down.'],
    [!I.fuse && (c.tookBaseKey || at('key-base') === 'inv'), 'The basement door is in the mudroom.'],

    [!I.keys && at('key-her') === 'inv', 'Upper hall, her door. Keys by the journal.'],
    [!I.keys && at('key-her') === 'stash', 'Her key is in the garage bin.'],
    [!I.keys && at('key-her') === 'floor', 'Her key is where you set it down.'],
    [!I.keys && at('key-car') === 'world' && c.herOpen, 'Her room, upper hall. Keys by the journal.'],
    [!I.keys && R.kitchen && at('key-her') === 'world' && !c.herOpen, 'Bathroom cabinet, left side. Her key.'],

    [R.diary && at('key-base') === 'world' && !c.tookBaseKey, "Mom's room. The key is in the sweater."],
    [!I.fuse && I.keys && R.foyer && at('key-base') === 'world' && !c.tookBaseKey, "Mom's room. The basement key is in the sweater."],
    [R.foyer && !I.keys, 'She has the car keys. The voice is not Mom.'],
    [c.hasLight && !R.foyer, 'Read the foyer note. The car is in the garage.'],
    [c.hasLight, 'The car is in the garage. It still will not start.'],
    [true, 'The foyer. Find a light.'],
  ];
  for (const [ok, text] of rules) if (ok) return text;
  return 'Get the car running.';
}
