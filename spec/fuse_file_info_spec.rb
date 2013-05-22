require 'spec_helper'

describe RFuse::Fuse do
    
    let(:dir_stat) { RFuse::Stat.directory(0444) }
    let(:file_stat) { RFuse::Stat.file(0444) }
    let(:mountpoint) { tempmount() }
    let(:open_files) { Hash.new() }
    let!(:mockfs) do
        m = mock("fuse")
        m.stub(:getattr).and_return(nil)
        m.stub(:getattr).with(anything(),"/").and_return(RFuse::Stat.directory(0777))
        m
    end
    
    it "should pass fileinfo to #release" do

        file_handle = Object.new()
        stored_ffi = nil
        captured_ex = nil

        mockfs.stub(:getattr).with(anything(),"/ffirelease").and_return(file_stat)

        mockfs.should_receive(:open).with(anything(),"/ffirelease",anything()) { |ctx,path,ffi|
           stored_ffi = ffi
           ffi.fh = file_handle 
        }
      
        mockfs.should_receive(:release).with(anything(),"/ffirelease",anything()) { |ctx,path,ffi|
            # the return value of release is ignore, so exceptions here are lost
            begin
                ffi.fh.should == file_handle
                ffi.should == stored_ffi
            rescue Exception => ex
                captured_ex = ex
            end
        }

        with_fuse(mountpoint,mockfs) do
            f1 = File.new("#{mountpoint}/ffirelease")
            f1.close()
        end

        raise captured_ex if captured_ex
    end

    context "file handles" do
        it "should retain file handles over GC" do

            file_stat.size = 11

            mockfs.stub(:getattr).with(anything(),"/one").and_return(file_stat)
            mockfs.stub(:getattr).with(anything(),"/two").and_return(file_stat)

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
