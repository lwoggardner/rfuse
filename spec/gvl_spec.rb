require 'spec_helper'

describe RFuse do
    context "release GVL" do
        it "should handle missing files" do
            $DEBUG = true

            mockfs = mock("fuse")
            mockfs.stub(:getattr).and_return(nil)
            file_stat = RFuse::Stat.file(0444)

            mockfs.stub(:getattr).with(anything(),"/missing").and_return(file_stat)

            thread_ran = false
            Thread.new() { sleep 0.5 ; puts "Look at Moi!" ; thread_ran = true }
            with_fuse("/tmp/rfuse-spec",mockfs,"-odebug") do
                sleep 2;
                File.stat("/tmp/rfuse-spec/missing");
            end
            thread_ran.should be_true
        end
    end
end

