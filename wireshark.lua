
do
   local p_nsdp = Proto("nsdp","Netgear Switch Discovery Protocol")

   local f_version  = ProtoField.uint8("nsdp.version","Version", base.DEC)
   local f_op = ProtoField.uint8("nsdp.op","Op", base.DEC)
   local f_host_mac = ProtoField.string("nsdp.host_mac","Host Mac")
   local f_dev_mac = ProtoField.string("nsdp.dev_mac","Device Mac")
   local f_seq_no = ProtoField.uint16("nsdp.seq_no","Sequence Number", base.DEC)
   local f_signature = ProtoField.string("nsdp.signature","Signature")
   --local f_seq_no = ProtoField.uint16("nsdp.seq_no","Sequence Number", base.DEC)
   --local f_host_mac = ProtoField.uint16("nsdp.host_mac","Host Mac", base.HEX)

   p_nsdp.fields = {
      f_version,
      f_op,
      f_host_mac,
      f_dev_mac,
      f_seq_no,
      f_signature,
   }

   function read_data(op, type, buf, pos, len, tlv)
      if op == 1 or op == 3 then
         if (len > 0) then
            tlv:append_text(" - Unknown data")
         end
      else -- response
         if type == 0x0001 or type == 0x0003 or type == 0x000D then
            -- string
            tlv:append_text(" (" .. buf(pos, len):string() .. ")")
         elseif type == 0x0004 then
            if len == 6 then
               tlv:append_text(string.format(" (%02x:%02x:%02x:%02x:%02x:%02x)",
                                             buf(pos+0,1):uint(),
                                             buf(pos+1,1):uint(),
                                             buf(pos+2,1):uint(),
                                             buf(pos+3,1):uint(),
                                             buf(pos+4,1):uint(),
                                             buf(pos+5,1):uint()))
            else
               tlv:append_text(" (invalid data)")
            end
         elseif (type >= 0x0006 and type <= 0x0008) or type == 0x000A then
            -- IP
            if len == 4 then
               tlv:append_text(" (" .. buf(pos+0,1):uint() .. "." ..
                               buf(pos+1,1):uint() .. "." ..
                               buf(pos+2,1):uint() .. "." ..
                               buf(pos+3,1):uint() .. ")")
            else
               tlv:append_text(" (invalid data)")
            end
         elseif type == 0x6000 then
            -- u8
            if len == 1 then
               tlv:append_text(" (" .. buf(pos,1):uint() .. ")")
            else
               tlv:append_text(" (invalid data)")
            end
         elseif type == 0x000B then
            -- Flag
            if (len == 1) then
               local v = buf(pos,1):uint()
               if v > 0  then
                  tlv:append_text(" (on)")
               else
                  tlv:append_text(" (off)")
               end
            else
               tlv:append_text(" (invalid data)")
            end
         elseif type == 0x0C00 then
            -- port status
            if (len == 3) then
               tlv:add(buf(pos,1), "Port " .. buf(pos,1):uint())
               local state_id = buf(pos+1,1):uint()
               local state = ""
               if state_id == 0 then
                  state = "Disconnected"
               elseif state_id < 3 then
                  state = "Up - 10M"
               elseif state_id < 5 then
                  state = "Up - 100M"
               elseif state_id == 5 then
                  state = "Up - 1G"
               else
                  state = "Unknown (" .. state_id .. ")"
               end
               tlv:add(buf(pos+1,1), "State: " .. state)
               tlv:add(buf(pos+2,1), "Unknown (" .. buf(pos+2,1):uint() .. ")")
            else
               tlv:add(buf(pos+4,len), " (invalid data)")
            end
         elseif len > 0 then
            -- Unknown stuff
            tlv:add(buf(pos,len), "Data")
         end
      end
   end

   function p_nsdp.dissector(buf,pkt,root)
      pkt.cols.protocol = "NSDP"
      local node = root:add(p_nsdp, buf(0))
      node:add(f_version,buf(0,1))
      node:add(f_op,buf(1,1))
      node:add(f_host_mac,buf(0x08,6))
      node:add(f_dev_mac,buf(0x0e,6))
      node:add(f_seq_no,buf(0x16,2))
      node:add(f_signature,buf(0x18,4))
      -- node:append_text(": Port " .. buf(1,1):uint())
      local op = buf(1,1):uint()
      local pos = 0x20
      local tlvs = node:add(buf(pos), "TLV List")
      local type = 0
      while type ~= 0xFFFF do
         type = buf(pos,2):uint()
         local len = buf(pos+2,2):uint()
         local tlv = tlvs:add(buf(pos,4+len), "TLV " .. type)
         if type == 0x0001 then
            tlv:append_text(" - Model")
         elseif type == 0x0003 then
            tlv:append_text(" - Hostname")
         elseif type == 0x0004 then
            tlv:append_text(" - MAC")
         elseif type == 0x0006 then
            tlv:append_text(" - IP")
         elseif type == 0x0007 then
            tlv:append_text(" - Netmask")
         elseif type == 0x0008 then
            tlv:append_text(" - Gateway")
         elseif type == 0x000A then
            tlv:append_text(" - Password")
         elseif type == 0x000B then
            tlv:append_text(" - DHCP")
         elseif type == 0x000D then
            tlv:append_text(" - Firmware Version")
         elseif type == 0x0C00 then
            tlv:append_text(" - Port status")
         elseif type == 0xFFFF then
            tlv:append_text(" - Terminator")
         end

         read_data(op, type, buf, pos+4, len, tlv)
         pos = pos + 4 + len
      end
   end

   local udp_table = DissectorTable.get("udp.port")

   --63321, 63322, 63323, 63324
   udp_table:add(63321, p_nsdp)
end

