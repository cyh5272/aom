-- automatically generated by the FlatBuffers compiler, do not modify

-- namespace: NamespaceB

local flatbuffers = require('flatbuffers')

local StructInNestedNS = {} -- the module
local StructInNestedNS_mt = {} -- the class metatable

function StructInNestedNS.New()
    local o = {}
    setmetatable(o, {__index = StructInNestedNS_mt})
    return o
end
function StructInNestedNS_mt:Init(buf, pos)
    self.view = flatbuffers.view.New(buf, pos)
end
function StructInNestedNS_mt:A()
    return self.view:Get(flatbuffers.N.Int32, self.view.pos + 0)
end
function StructInNestedNS_mt:B()
    return self.view:Get(flatbuffers.N.Int32, self.view.pos + 4)
end
function StructInNestedNS.CreateStructInNestedNS(builder, a, b)
    builder:Prep(4, 8)
    builder:PrependInt32(b)
    builder:PrependInt32(a)
    return builder:Offset()
end

return StructInNestedNS -- return the module