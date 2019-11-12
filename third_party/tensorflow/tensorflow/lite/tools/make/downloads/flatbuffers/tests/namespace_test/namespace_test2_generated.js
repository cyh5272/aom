// automatically generated by the FlatBuffers compiler, do not modify

/**
 * @const
 * @namespace
 */
var NamespaceA = NamespaceA || {};

/**
 * @const
 * @namespace
 */
NamespaceA.NamespaceB = NamespaceA.NamespaceB || {};

/**
 * @const
 * @namespace
 */
var NamespaceC = NamespaceC || {};

/**
 * @constructor
 */
NamespaceA.TableInFirstNS = function() {
  /**
   * @type {flatbuffers.ByteBuffer}
   */
  this.bb = null;

  /**
   * @type {number}
   */
  this.bb_pos = 0;
};

/**
 * @param {number} i
 * @param {flatbuffers.ByteBuffer} bb
 * @returns {NamespaceA.TableInFirstNS}
 */
NamespaceA.TableInFirstNS.prototype.__init = function(i, bb) {
  this.bb_pos = i;
  this.bb = bb;
  return this;
};

/**
 * @param {flatbuffers.ByteBuffer} bb
 * @param {NamespaceA.TableInFirstNS=} obj
 * @returns {NamespaceA.TableInFirstNS}
 */
NamespaceA.TableInFirstNS.getRootAsTableInFirstNS = function(bb, obj) {
  return (obj || new NamespaceA.TableInFirstNS).__init(bb.readInt32(bb.position()) + bb.position(), bb);
};

/**
 * @param {NamespaceA.NamespaceB.TableInNestedNS=} obj
 * @returns {NamespaceA.NamespaceB.TableInNestedNS|null}
 */
NamespaceA.TableInFirstNS.prototype.fooTable = function(obj) {
  var offset = this.bb.__offset(this.bb_pos, 4);
  return offset ? (obj || new NamespaceA.NamespaceB.TableInNestedNS).__init(this.bb.__indirect(this.bb_pos + offset), this.bb) : null;
};

/**
 * @returns {NamespaceA.NamespaceB.EnumInNestedNS}
 */
NamespaceA.TableInFirstNS.prototype.fooEnum = function() {
  var offset = this.bb.__offset(this.bb_pos, 6);
  return offset ? /** @type {NamespaceA.NamespaceB.EnumInNestedNS} */ (this.bb.readInt8(this.bb_pos + offset)) : NamespaceA.NamespaceB.EnumInNestedNS.A;
};

/**
 * @param {NamespaceA.NamespaceB.EnumInNestedNS} value
 * @returns {boolean}
 */
NamespaceA.TableInFirstNS.prototype.mutate_foo_enum = function(value) {
  var offset = this.bb.__offset(this.bb_pos, 6);

  if (offset === 0) {
    return false;
  }

  this.bb.writeInt8(this.bb_pos + offset, value);
  return true;
};

/**
 * @param {NamespaceA.NamespaceB.StructInNestedNS=} obj
 * @returns {NamespaceA.NamespaceB.StructInNestedNS|null}
 */
NamespaceA.TableInFirstNS.prototype.fooStruct = function(obj) {
  var offset = this.bb.__offset(this.bb_pos, 8);
  return offset ? (obj || new NamespaceA.NamespaceB.StructInNestedNS).__init(this.bb_pos + offset, this.bb) : null;
};

/**
 * @param {flatbuffers.Builder} builder
 */
NamespaceA.TableInFirstNS.startTableInFirstNS = function(builder) {
  builder.startObject(3);
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} fooTableOffset
 */
NamespaceA.TableInFirstNS.addFooTable = function(builder, fooTableOffset) {
  builder.addFieldOffset(0, fooTableOffset, 0);
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {NamespaceA.NamespaceB.EnumInNestedNS} fooEnum
 */
NamespaceA.TableInFirstNS.addFooEnum = function(builder, fooEnum) {
  builder.addFieldInt8(1, fooEnum, NamespaceA.NamespaceB.EnumInNestedNS.A);
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} fooStructOffset
 */
NamespaceA.TableInFirstNS.addFooStruct = function(builder, fooStructOffset) {
  builder.addFieldStruct(2, fooStructOffset, 0);
};

/**
 * @param {flatbuffers.Builder} builder
 * @returns {flatbuffers.Offset}
 */
NamespaceA.TableInFirstNS.endTableInFirstNS = function(builder) {
  var offset = builder.endObject();
  return offset;
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} fooTableOffset
 * @param {NS8755221360535654258.NamespaceA.NamespaceB.EnumInNestedNS} fooEnum
 * @param {flatbuffers.Offset} fooStructOffset
 * @returns {flatbuffers.Offset}
 */
NamespaceA.TableInFirstNS.createTableInFirstNS = function(builder, fooTableOffset, fooEnum, fooStructOffset) {
  NamespaceA.TableInFirstNS.startTableInFirstNS(builder);
  NamespaceA.TableInFirstNS.addFooTable(builder, fooTableOffset);
  NamespaceA.TableInFirstNS.addFooEnum(builder, fooEnum);
  NamespaceA.TableInFirstNS.addFooStruct(builder, fooStructOffset);
  return NamespaceA.TableInFirstNS.endTableInFirstNS(builder);
}

/**
 * @constructor
 */
NamespaceC.TableInC = function() {
  /**
   * @type {flatbuffers.ByteBuffer}
   */
  this.bb = null;

  /**
   * @type {number}
   */
  this.bb_pos = 0;
};

/**
 * @param {number} i
 * @param {flatbuffers.ByteBuffer} bb
 * @returns {NamespaceC.TableInC}
 */
NamespaceC.TableInC.prototype.__init = function(i, bb) {
  this.bb_pos = i;
  this.bb = bb;
  return this;
};

/**
 * @param {flatbuffers.ByteBuffer} bb
 * @param {NamespaceC.TableInC=} obj
 * @returns {NamespaceC.TableInC}
 */
NamespaceC.TableInC.getRootAsTableInC = function(bb, obj) {
  return (obj || new NamespaceC.TableInC).__init(bb.readInt32(bb.position()) + bb.position(), bb);
};

/**
 * @param {NamespaceA.TableInFirstNS=} obj
 * @returns {NamespaceA.TableInFirstNS|null}
 */
NamespaceC.TableInC.prototype.referToA1 = function(obj) {
  var offset = this.bb.__offset(this.bb_pos, 4);
  return offset ? (obj || new NamespaceA.TableInFirstNS).__init(this.bb.__indirect(this.bb_pos + offset), this.bb) : null;
};

/**
 * @param {NamespaceA.SecondTableInA=} obj
 * @returns {NamespaceA.SecondTableInA|null}
 */
NamespaceC.TableInC.prototype.referToA2 = function(obj) {
  var offset = this.bb.__offset(this.bb_pos, 6);
  return offset ? (obj || new NamespaceA.SecondTableInA).__init(this.bb.__indirect(this.bb_pos + offset), this.bb) : null;
};

/**
 * @param {flatbuffers.Builder} builder
 */
NamespaceC.TableInC.startTableInC = function(builder) {
  builder.startObject(2);
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} referToA1Offset
 */
NamespaceC.TableInC.addReferToA1 = function(builder, referToA1Offset) {
  builder.addFieldOffset(0, referToA1Offset, 0);
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} referToA2Offset
 */
NamespaceC.TableInC.addReferToA2 = function(builder, referToA2Offset) {
  builder.addFieldOffset(1, referToA2Offset, 0);
};

/**
 * @param {flatbuffers.Builder} builder
 * @returns {flatbuffers.Offset}
 */
NamespaceC.TableInC.endTableInC = function(builder) {
  var offset = builder.endObject();
  return offset;
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} referToA1Offset
 * @param {flatbuffers.Offset} referToA2Offset
 * @returns {flatbuffers.Offset}
 */
NamespaceC.TableInC.createTableInC = function(builder, referToA1Offset, referToA2Offset) {
  NamespaceC.TableInC.startTableInC(builder);
  NamespaceC.TableInC.addReferToA1(builder, referToA1Offset);
  NamespaceC.TableInC.addReferToA2(builder, referToA2Offset);
  return NamespaceC.TableInC.endTableInC(builder);
}

/**
 * @constructor
 */
NamespaceA.SecondTableInA = function() {
  /**
   * @type {flatbuffers.ByteBuffer}
   */
  this.bb = null;

  /**
   * @type {number}
   */
  this.bb_pos = 0;
};

/**
 * @param {number} i
 * @param {flatbuffers.ByteBuffer} bb
 * @returns {NamespaceA.SecondTableInA}
 */
NamespaceA.SecondTableInA.prototype.__init = function(i, bb) {
  this.bb_pos = i;
  this.bb = bb;
  return this;
};

/**
 * @param {flatbuffers.ByteBuffer} bb
 * @param {NamespaceA.SecondTableInA=} obj
 * @returns {NamespaceA.SecondTableInA}
 */
NamespaceA.SecondTableInA.getRootAsSecondTableInA = function(bb, obj) {
  return (obj || new NamespaceA.SecondTableInA).__init(bb.readInt32(bb.position()) + bb.position(), bb);
};

/**
 * @param {NamespaceC.TableInC=} obj
 * @returns {NamespaceC.TableInC|null}
 */
NamespaceA.SecondTableInA.prototype.referToC = function(obj) {
  var offset = this.bb.__offset(this.bb_pos, 4);
  return offset ? (obj || new NamespaceC.TableInC).__init(this.bb.__indirect(this.bb_pos + offset), this.bb) : null;
};

/**
 * @param {flatbuffers.Builder} builder
 */
NamespaceA.SecondTableInA.startSecondTableInA = function(builder) {
  builder.startObject(1);
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} referToCOffset
 */
NamespaceA.SecondTableInA.addReferToC = function(builder, referToCOffset) {
  builder.addFieldOffset(0, referToCOffset, 0);
};

/**
 * @param {flatbuffers.Builder} builder
 * @returns {flatbuffers.Offset}
 */
NamespaceA.SecondTableInA.endSecondTableInA = function(builder) {
  var offset = builder.endObject();
  return offset;
};

/**
 * @param {flatbuffers.Builder} builder
 * @param {flatbuffers.Offset} referToCOffset
 * @returns {flatbuffers.Offset}
 */
NamespaceA.SecondTableInA.createSecondTableInA = function(builder, referToCOffset) {
  NamespaceA.SecondTableInA.startSecondTableInA(builder);
  NamespaceA.SecondTableInA.addReferToC(builder, referToCOffset);
  return NamespaceA.SecondTableInA.endSecondTableInA(builder);
}

// Exports for Node.js and RequireJS
this.NamespaceA = NamespaceA;
this.NamespaceC = NamespaceC;
