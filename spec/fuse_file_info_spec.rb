require 'spec_helper'

describe RFuse::Fuse do

    let(:dir_stat) { RFuse::Stat.directory(0444) }
    let(:file_stat) { RFuse::Stat.file(0444) }
    let!(:mockfs) { m = mock("fuse"); m.stub(:getattr).and_return(nil); m }
    let(:mountpoint) { tempmount() }
    let(:open_files) { Hash.new() }

    it "should pass fileinfo to #release" do

        file_handle = Object.new()
        stored_ffi = nil
        captured_ex = nil

        mockfs.stub(:getattr).with(anything(),"/ffirelease").and_return(file_stat)

        mockfs.should_receive(:open).with(anything(),"/ffirelease",anything()) { |ctx,path,ffi|
           stored_ffi = ffi
           ffi.fh = file_handle
           ctx.uid.should > 0
        }

        mockfs.should_receive(:release).with(anything(),"/ffirelease",anything()) { |ctx,path,ffi|
            # the return value of release is ignored, so exceptions here are lost
            begin
                ffi.fh.should == file_handle
                ffi.should == stored_ffi
                # Not sure why ctx.uid is not still set during release
                ctx.uid.should == 0
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

    it "should pass fileinfo to #releasedir" do

        file_handle = Object.new()
        stored_ffi = nil
        captured_ex = nil

        mockfs.stub(:getattr).with(anything(),"/ffirelease").and_return(dir_stat)

        mockfs.should_receive(:opendir).with(anything(),"/ffirelease",anything()) { |ctx,path,ffi|
            stored_ffi = ffi
            ffi.fh = file_handle
            ctx.uid.should > 0
        }

        mockfs.should_receive(:readdir) do | ctx, path, filler,offset,ffi |
            filler.push("hello",nil,0)
            filler.push("world",nil,0)
        end

        mockfs.should_receive(:releasedir).with(anything(),"/ffirelease",anything()) { |ctx,path,ffi|
            # the return value of release is ignored, so exceptions here are lost
            begin
                ffi.fh.should == file_handle
                ffi.should == stored_ffi
                # Not entirely sure why ctx.uid is not set here
                ctx.uid.should == 0
            rescue Exception => ex
                captured_ex = ex
            end
        }

        with_fuse(mountpoint,mockfs) do
            entries = Dir.entries("#{mountpoint}/ffirelease")
            entries.size.should == 2
            entries.should include("hello")
            entries.should include("world")
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
