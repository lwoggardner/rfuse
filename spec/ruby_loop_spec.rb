require 'spec_helper'

describe RFuse::Fuse do

    let(:mockfs) { m = mock("fuse"); m.stub(:getattr).and_return(nil) ; m }
    let(:mountpoint) { tempmount() }

    context "#loop" do
        it "should exit from another thread and allow multiple loops" do

            fuse = RFuse::FuseDelegator.new(mockfs,mountpoint)
            t = Thread.new { sleep 0.2;  fuse.exit }
            fuse.loop()
            t.join
            t = Thread.new { sleep 0.2;  fuse.exit }
            fuse.loop
            t.join
            fuse.unmount
            fuse.mounted?.should be(false)
        end


        # This will never work!
        #it "should allow threads to operate on the filesystem" do
        #
        #    mockfs = mock("fuse")
        #    mockfs.stub(:getattr).and_return(nil)
        #
        #    fuse = RFuse::FuseDelegator.new(mockfs,"/tmp/rfuse-spec")
        #
        #    t = Thread.new { sleep 0.5 ; File.stat("/tmp/rfuse-spec/thread") ; fuse.exit }
        #
        #    fuse.loop()
        #    fuse.unmount()
        #end

        it "should allow other threads to be scheduled" do

            file_stat = RFuse::Stat.file(0444)

            thread_ran = false

            mockfs.stub(:getattr).with(anything(),"/before") {
                thread_ran.should be(false)
                file_stat
            }

            mockfs.stub(:getattr).with(anything(),"/missing") {
                #GC.start()
                thread_ran.should be(true)
                file_stat
            }

            t = Thread.new() { sleep 0.5 ; thread_ran = true }
            with_fuse(mountpoint,mockfs) do
                File.stat("#{mountpoint}/before");
                sleep 1;
                File.stat("#{mountpoint}/missing");
            end
        end
    end
end

