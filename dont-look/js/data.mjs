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
    desc: 'Still taped from the game. Quiet, until the tape gives.',
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
    desc: 'Short, sharp, and not going to last.',
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
    desc: 'Loud enough to call the whole house. The street is not there to hear it.',
  },
  ammo: {
    name: 'Rounds',
    type: 'ammo',
    stack: 12,
    desc: 'Not many. Spending one should mean something.',
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
    desc: 'The date on the bottle is fine. Everything else is not.',
  },
  film: {
    name: 'Film Slide',
    type: 'film',
    stack: 3,
    desc: 'One exposure. The garage bench can pin a room in place. There are not many.',
  },
  tape: {
    name: 'Stick Tape',
    type: 'tape',
    desc: 'Wrap the hockey stick before it gives up.',
  },
  'key-her': {
    name: 'Brass Key',
    type: 'key',
    desc: 'From the bathroom cabinet. It fits her door, off the hall.',
  },
  'key-base': {
    name: 'Basement Key',
    type: 'key',
    desc: 'Warm from the sweater pocket. The basement door is in the mudroom.',
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
    desc: 'The garage door motor is dead without it. Seat it in the car.',
  },
  gas: {
    name: 'Gas Can',
    type: 'quest',
    desc: 'Heavy. The gauge was on E. So is everything else.',
  },
  camera: {
    name: "Ben's Camera",
    type: 'camera',
    flashes: 3,
    desc: 'The bulbs stun. The plate in the back is for the car — not the face.',
  },
};

export const ENEMY = {
  shambler: { hp: 56, speed: 62, dmg: 12, reach: 16, radius: 10 },
  grin: { hp: 96, speed: 118, dmg: 26, reach: 20, radius: 12, revive: 16 },
};

export const MOM_LINES = [
  'Chris? The door is open, honey.',
  "I'm in my room. Don't turn on the light.",
  'I kept a plate warm for you.',
  "Come closer. I can't see you from here.",
  "You looked at it, didn't you.",
  'Ben said you would.',
  'Come upstairs.',
  'I have the blue sweater on. Come see.',
];

export const NOTES = {
  foyer: {
    title: "Mom's note",
    body: "Chris —\n\nShe has your keys again. The flashlight is on this table if you walked past it.\n\nThe basement key is in my room, sweater pocket. If you already hear me talking, that isn't me. I'm in the car. Don't answer.\n\nIf your hands fill up, the bin in the garage holds the rest.",
  },
  kitchen: {
    title: 'Grocery list',
    body: "milk, batteries, the good bread\n\nleft side of the bathroom cabinet. her key. it sticks.",
  },
  hockey: {
    title: 'Desk',
    body: "Thursday. We won. Film in the drawer, tape next to it, two rounds I shouldn't have kept.\n\nIf the car won't leave, the garage bench takes a slide. There are not many slides.",
  },
  journal: {
    title: 'Her journal',
    body: "Mom said stay in the car. The gauge was on E, so I went for the can. It's on the shed shelf. The yard door wouldn't lift. No power.\n\nI heard Ben in the basement, in the tanks. It wasn't his voice for long.\n\nIf you're reading this, get the gas, then don't look at the water.",
  },
  diary: {
    title: "Mom's diary",
    body: "Ben called before midnight. He was crying. He said the picture from the trail opened, and that he sent it to you anyway.\n\nI'm going to sit in the car with the doors locked. If the house says my name, it learned it from the photo.\n\nBasement key is in the sweater. Don't pick it up if the bed is breathing.",
  },
  ray: {
    title: 'Scratched note',
    body: "I pulled the fuse so the yard door couldn't open and it couldn't see the grass.\n\nFuse is on this bench. Camera too. Bulbs stun it. The plate in the back is only for the car — photograph the car, not the face.\n\nShed key is on the ring. The name on the ring is gone.",
  },
  bag: {
    title: 'On the lock screen',
    photo: 'teammate',
    body: "you left this in my car.\n\nalso don't open ben's picture.\n\ni looked.\ni'm fine.\ni'm fine.",
  },
};

export const INSPECTIONS = {
  duffel: {
    title: 'Your bag',
    body: "The stick isn't in it. You dropped it by the couch. The rest of the gear is in the garage.",
  },
  front: {
    title: 'Front door',
    body: 'The door is here. The street is not. The wood is warm, like someone breathing on the other side.',
  },
  family: {
    title: 'The yard, before',
    body: "Three people and a normal afternoon. If the picture starts to move, put it down.",
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
    title: 'A photo on the dresser',
    body: "She's laughing. The shirt is the brightest thing in the house. Nothing else is that color anymore.",
    photos: [{ id: 'her', crop: 'bust', label: '' }],
  },
  mirror: {
    title: 'Mirror',
    body: "DONT FOLLOW HER VOICE\n\nWritten in lipstick. The mouth in the glass isn't yours.",
    photos: [{ id: 'chris', crop: 'face', label: 'You', grin: 0.45, corrupt: 0.7 }],
  },
  mompic: {
    title: 'Nightstand',
    body: "Mom, in the yard. The sweater in the picture is the one on the bed. She isn't in it.",
    photos: [{ id: 'mom', crop: 'bust', label: 'Mom' }],
  },
  badge: {
    title: 'A photo, face up',
    body: 'The name is scratched out. The tanks behind him are the ones still running.',
    photos: [{ id: 'basement', crop: 'bust', label: '' }],
  },
  tanks: {
    title: 'The tanks',
    body: "They haven't stopped humming. The glass would rather show the other face.",
    photos: [
      { id: 'basement', crop: 'face', label: 'Then' },
      { id: 'grin', crop: 'face', label: 'Now' },
    ],
  },
  yard: {
    title: 'The yard',
    body: 'The grass is the right shape and the wrong color. The sky looks like a bruise that learned your address.',
  },
  cloth: {
    title: 'Torn cloth',
    body: "Turquoise, caught on the shelf. The can is still here. She isn't.",
    photos: [{ id: 'her', crop: 'face', label: '' }],
  },
};

const held = (loc, id) => loc[id] === 'inv' || loc[id] === 'stash' || loc[id] === 'floor';

export function describeGoal(c) {
  const I = c.installed;
  const R = c.read || {};
  const L = c.loc;
  const rules = [
    [c.finale, 'Hold E at the car. Do not let it in with you.'],
    [c.room === 'her' && !I.keys && L['key-car'] === 'world', 'The keys are on the desk.'],
    [c.room === 'shed' && !I.gas, 'The can is on the shelf.'],
    [c.room === 'basement' && !I.fuse, 'Bench: fuse, camera, shed key. The revolver is optional. Loud.'],
    [I.fuse && I.keys && I.gas && !I.photo && held(L, 'camera'), "Photograph the car. Not the face."],
    [I.fuse && I.keys && I.gas && !I.photo, "Ben's camera is on the basement bench. One picture of the car."],
    [!I.fuse && held(L, 'fuse'), 'Seat the fuse in the car. It opens the yard.'],
    [!I.gas && held(L, 'gas'), 'Put the gas can in the car.'],
    [!I.keys && held(L, 'key-car'), 'The car keys go in the ignition.'],
    [I.fuse && !I.gas && (R.journal || R.ray), 'Shed in the yard. The can is on the shelf. The ring key opens it.'],
    [I.fuse && !I.gas, 'The yard door is open. Find fuel.'],
    [!I.fuse && R.ray, 'Basement bench: fuse, camera, shed key.'],
    [!I.fuse && (c.tookBaseKey || L['key-base'] !== 'world'), 'Mudroom. The basement door. Cold air under it.'],
    [!I.keys && held(L, 'key-her'), 'Her door is off the hall. The keys are on the desk.'],
    [!I.keys && R.kitchen, 'Bathroom cabinet, left side. Her key.'],
    [R.diary && L['key-base'] === 'world' && !c.tookBaseKey, "Sweater pocket, Mom's room. The bed is breathing."],
    [R.foyer && !I.keys, 'She has the car keys. The voice in the house is not Mom.'],
    [c.hasLight, "Mom's voice is down the hall. The car is in the garage."],
    [true, 'The foyer table. Find a light.'],
  ];
  for (const [ok, text] of rules) if (ok) return text;
  return 'Get the car running.';
}
