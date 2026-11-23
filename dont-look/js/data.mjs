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
    desc: 'Taped from tonight\'s game. Quiet, until the tape gives.',
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
    desc: 'From the kitchen block. The edge is already nicked.',
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
    desc: 'One shot is loud enough for the whole house. There is no street left to hear it.',
  },
  ammo: {
    name: 'Rounds',
    type: 'ammo',
    stack: 12,
    desc: 'A few rounds. Spend one only if you mean it.',
  },
  bandage: {
    name: 'Bandage',
    type: 'heal',
    heal: 34,
    stack: 4,
    desc: 'Enough to stop the shaking and keep walking.',
  },
  pills: {
    name: 'Painkillers',
    type: 'heal',
    heal: 78,
    stack: 2,
    desc: 'The date on the bottle is still good. The rest of the house is not.',
  },
  film: {
    name: 'Film Slide',
    type: 'film',
    stack: 3,
    desc: 'One exposure. The garage bench can hold this room. There are not many slides.',
  },
  tape: {
    name: 'Stick Tape',
    type: 'tape',
    desc: 'Wrap the hockey stick before the shaft splits.',
  },
  'key-her': {
    name: 'Brass Key',
    type: 'key',
    desc: 'From the left side of the bathroom cabinet. It opens her door, off the hall.',
  },
  'key-base': {
    name: 'Basement Key',
    type: 'key',
    desc: 'Still warm from the sweater pocket. The basement door is in the mudroom.',
  },
  'key-shed': {
    name: 'Shed Key',
    type: 'key',
    desc: 'A ring with the name scratched off. It opens the shed in the yard.',
  },
  'key-car': {
    name: 'Car Keys',
    type: 'key',
    desc: 'Yours. She had them again. They go in the ignition.',
  },
  fuse: {
    name: 'Fuse',
    type: 'quest',
    desc: 'The yard door will not lift without it. It goes in the car.',
  },
  gas: {
    name: 'Gas Can',
    type: 'quest',
    desc: 'Heavy. The gauge was on empty. Pour it into the car.',
  },
  camera: {
    name: "Ben's Camera",
    type: 'camera',
    flashes: 3,
    desc: 'The bulbs stun whatever is wearing the smile. The plate in the back is for a photo of the car, not the face.',
  },
};

export const ENEMY = {
  shambler: { hp: 56, speed: 62, dmg: 12, reach: 16, radius: 10 },
  grin: { hp: 96, speed: 118, dmg: 26, reach: 20, radius: 12, revive: 16 },
};

export const MOM_LINES = [
  'Chris? The door is open, honey.',
  "I'm in my room. Don't turn the light on.",
  'I kept a plate warm for you.',
  "Come closer. I can't see you from the hall.",
  "You looked at it, didn't you.",
  'Ben said you would.',
  'Come upstairs.',
  'I have the blue sweater on. Come see.',
];

export const NOTES = {
  foyer: {
    title: "Mom's note",
    body: "Chris —\n\nShe has your keys again. If you walked past the flashlight, it is on this table.\n\nThe basement key is in my room, in the sweater pocket. If you can already hear me talking, that is not me. I am in the car. Do not answer.\n\nIf your hands fill up, the bin in the garage will hold the rest.",
  },
  kitchen: {
    title: 'Grocery list',
    body: "milk\nbatteries\nthe good bread\n\nHer key is on the left side of the bathroom cabinet. It sticks.",
  },
  hockey: {
    title: 'Desk',
    body: "Thursday. We won.\n\nThe film is in the drawer. The tape is next to it. So are two rounds I should not have kept.\n\nIf the car will not leave, the bench in the garage takes one slide. There are not many slides.",
  },
  journal: {
    title: 'Her journal',
    body: "Mom said to stay in the car. The gauge was on empty, so I went for the can. It is on the shelf in the shed. The yard door would not lift. There is no power.\n\nI heard Ben in the basement, in the tanks. It was not his voice for long.\n\nIf you are reading this, get the gas. Then do not look at the water.",
  },
  diary: {
    title: "Mom's diary",
    body: "Ben called before midnight. He was crying. He said the trail picture opened, and that he sent it to you anyway.\n\nI am going to sit in the car with the doors locked. If the house says my name, it learned that name from the photo.\n\nThe basement key is in the sweater. Do not take it if the bed is breathing.",
  },
  ray: {
    title: 'Scratched note',
    body: "I pulled the fuse so the yard door could not open, and so it could not see the grass.\n\nThe fuse is on this bench. Ben's camera is here too. The bulbs stun it. The plate in the back is only for the car. Photograph the car, not the face.\n\nThe shed key is on the ring. The name on the ring is gone.",
  },
  bag: {
    title: 'On the lock screen',
    photo: 'teammate',
    body: "You left this in my car.\n\nAlso: do not open Ben's picture.\n\nI looked.\nI'm fine.\nI'm fine.",
  },
};

export const INSPECTIONS = {
  duffel: {
    title: 'Your bag',
    body: 'The stick is not in it. You dropped it by the couch. The rest of the gear is in the garage.',
  },
  front: {
    title: 'Front door',
    body: 'The door is here. The street is not. The wood is warm, as if someone is breathing on the other side.',
  },
  family: {
    title: 'The yard, before',
    body: 'Three people, and a normal afternoon. If the picture starts to move, put it down.',
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
    body: 'She is laughing. The turquoise shirt is the brightest thing left in the house.',
    photos: [{ id: 'her', crop: 'bust', label: '' }],
  },
  mirror: {
    title: 'Mirror',
    body: "DONT FOLLOW HER VOICE\n\nWritten in lipstick. The mouth in the glass is not yours.",
    photos: [{ id: 'chris', crop: 'face', label: 'You', grin: 0.45, corrupt: 0.7 }],
  },
  mompic: {
    title: 'Nightstand',
    body: 'Mom, in the yard. The blue sweater in the picture is the one on the bed. She is not in it.',
    photos: [{ id: 'mom', crop: 'bust', label: 'Mom' }],
  },
  badge: {
    title: 'A photo, face up',
    body: 'The name is scratched out. The tanks behind him are the ones still running.',
    photos: [{ id: 'basement', crop: 'bust', label: '' }],
  },
  tanks: {
    title: 'The tanks',
    body: 'They have not stopped humming. The glass would rather show the other face.',
    photos: [
      { id: 'basement', crop: 'face', label: 'Then' },
      { id: 'grin', crop: 'face', label: 'Now' },
    ],
  },
  yard: {
    title: 'The yard',
    body: 'The grass is the right shape and the wrong color. The sky looks like a bruise that learned this address.',
  },
  cloth: {
    title: 'Torn cloth',
    body: 'Turquoise, caught on the shelf. The gas can is still here. She is not.',
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
    [c.room === 'her' && !I.keys && L['key-car'] === 'world', 'The car keys are on the desk.'],
    [c.room === 'shed' && !I.gas, 'The gas can is on the shelf.'],
    [c.room === 'basement' && !I.fuse, 'On the bench: the fuse, the camera, and the shed key. The revolver is loud.'],
    [I.fuse && I.keys && I.gas && !I.photo && held(L, 'camera'), 'Photograph the car. Not the face.'],
    [I.fuse && I.keys && I.gas && !I.photo, "Ben's camera is on the basement bench. Take one picture of the car."],
    [!I.fuse && held(L, 'fuse'), 'Put the fuse in the car. That powers the yard door.'],
    [!I.gas && held(L, 'gas'), 'Pour the gas into the car.'],
    [!I.keys && held(L, 'key-car'), 'The car keys go in the ignition.'],
    [I.fuse && !I.gas && (R.journal || R.ray), 'The shed is in the yard. The can is on the shelf. The ring key opens it.'],
    [I.fuse && !I.gas, 'The yard door is open. Find the gas.'],
    [!I.fuse && R.ray, 'The basement bench has the fuse, the camera, and the shed key.'],
    [!I.fuse && (c.tookBaseKey || L['key-base'] === 'inv' || L['key-base'] === 'stash'), 'The basement door is in the mudroom. Cold air comes under it.'],
    [!I.keys && held(L, 'key-her'), 'Her door is off the hall. The car keys are on the desk.'],
    [!I.keys && R.kitchen, 'Her key is on the left side of the bathroom cabinet.'],
    [R.diary && L['key-base'] === 'world' && !c.tookBaseKey, "The basement key is in the sweater, in Mom's room. The bed is breathing."],
    [R.foyer && !I.keys, 'She has the car keys. The voice in the house is not Mom.'],
    [c.hasLight, "Mom's voice is down the hall. The car is in the garage."],
    [true, 'Find the flashlight on the foyer table.'],
  ];
  for (const [ok, text] of rules) if (ok) return text;
  return 'Get the car running.';
}
