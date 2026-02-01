# Ryu
Ryu means dragon in Japanese.

Ryu is a data-oriented design (DOD) based entity-component management system. SoA (structure of arrays) and handle validity are some of the main focuses of Ryu. Most of the time, handles are passed as integers instead of pointers to ensure validity in case of memory re-allocation.

## Structure
### handles
handles are values returned by generator functions. Handles are unsigned integers of different sizes. Some of the bits of the handle are for the ID of the object, and the rest of the generation counter.

### generation counting
Since handles are recycled, we need to know if two same IDs belong to the same object or not. Imagine having a handle to an object. The object gets destroyed, and then recycled. Your previous handle is still pointing to that ID, but this time, it's a new object. That's why we increase the generation counter every time an object is generated.

#### handle validity
The validity of a handle can be checked by seeing first if the ID of handle is not free, and then checking if the generation count of the ID is the same as the one in the handle. However, since handles can only contain IDs to a maximum value based on their size, the max possible value is always considered to be invalid. For example in case of an 8-bit ID, the value 255 is considered to be invalid. This means that if the count of objects reaches the max value, the last object would never be used.

### Worlds
In Ryu, we have worlds. Worlds act as domains for entities. They isolate entities for different purposes. You can have two worlds, each for a level of the game and work on them simultanously, or have each session of an FPS game as a different world in the server.<br />
World handles are 16-bit unsigned integers. 8 for entity ID and 8 for the generation counter.

### Entities
Entities are the units of existence in Ryu. Each entity is identified by its ID, and the world which it belongs to. Entities have no behaviour by themselves and simply exist. Entity handles are 64-bit unsigned integers with 32 bits for ID, 16 for generation counting and 16 for the world handle. One can extract the world handle directly from the entity.

### Flush
Ryu, does not destroy entities or remove components from entities immidately. Instead, it stores them in a list and waits until flush points. At flush points, Ryu walks over the list, removes the components and then destroys the entities that are pending to be destroyed.