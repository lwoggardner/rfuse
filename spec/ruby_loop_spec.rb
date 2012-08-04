require 'spec_helper'

describe RFuse do
    context "ruby loop" do
        it "should exit from another thread and allow multiple loops" do
            mockfs = mock("fuse")
            mockfs.stub(:getattr).and_return(nil)
            
            fuse = RFuse::FuseDelegator.new(mockfs,"/tmp/rfuse-spec")
            t = Thread.new { sleep 0.5;  fuse.exit }
            fuse.loop()
            t.join
            t = Thread.new { sleep 0.5;  fuse.exit }
            fuse.loop
            t.join
            fuse.unmount
            fuse.mounted?.should be_false
        end


        #it "should allow threads to operate on the filesystem" do
        #
        #    mockfs = mock("fuse")
        #    mockfs.stub(:getattr).and_return(nil)

        #    fuse = RFuse::FuseDelegator.new(mockfs,"/tmp/rfuse-spec")

        #    t = Thread.new { sleep 0.5 ; File.stat("/tmp/rfuse-spec/thread") ; fuse.exit }
            
        #    fuse.loop()
        #    fuse.unmount()
        #end

        it "should handle missing files" do
            mockfs = mock("fuse")
            mockfs.stub(:getattr).and_return(nil)
            file_stat = RFuse::Stat.file(0444)

            mockfs.stub(:getattr).with(anything(),"/missing") {
                GC.start()
                file_stat
            }

            thread_ran = false
            Thread.new() { sleep 0.5 ; thread_ran = true }
            with_fuse("/tmp/rfuse-spec",mockfs,) do
                sleep 2;
                File.stat("/tmp/rfuse-spec/missing");
            end
            thread_ran.should be_true
        end
    end

end

