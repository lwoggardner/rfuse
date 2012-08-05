require 'spec_helper'

describe RFuse::Fuse do
    context "file handles" do
        it "should retain file handles over GC" do
            mockfs = mock("fuse")

            file_stat = RFuse::Stat.file(0444,:size => 11)

            mockfs.stub(:getattr) { | ctx, path|
                case path 
                when "/one","/two"
                    file_stat
                else
                    nil
                end

            }

            open_files = {}
            mockfs.stub(:open) { |ctx,path,ffi|
                ffi.fh = Object.new()
                open_files[path] = ffi.fh.object_id
            }

            mockfs.stub(:read) { |ctx,path,size,offset,ffi|
                GC.start()
                open_files[path].should == ffi.fh.object_id
                "hello world"
            }

            mountpoint = tempmount()
            with_fuse(mountpoint,mockfs) do
                f1 = File.new("#{mountpoint}/one")
                f2 = File.new("#{mountpoint}/two")

                val = f1.gets
                val.should == "hello world" 
                f2.gets.should == "hello world"

                f1.close()
                f2.close()
            end
        end
    end
end
