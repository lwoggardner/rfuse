require 'spec_helper'

class MockFuse < RFuse::Fuse; end
class DuckFuse
    def initialize(val); end;
end

describe RFuse do

    # required to capture stderr output of help text from C: library functions
    matcher :print_to_stderr do |expected|
        supports_block_expectations

        match do |actual|
            #$stdout.puts "STDERR=#{$stderr} #{$stderr.fileno}"
            @captured = File.open(Dir.tmpdir + "/rfuse.stderr","w+") do |f|
                f.sync
                old_stderr = STDERR.dup
                old_f = f.dup
                begin
                    STDERR.reopen(f)
                    actual.call
                ensure
                    STDERR.reopen(old_stderr)
                end
                old_f.rewind
                old_f.read
            end
            expected.match(@captured)
        end

        match_when_negated do |actual|
            !matches?(actual)
        end

        failure_message do
            "Expected stderr output...\n#{@captured}\nto match RE - #{expected}"
        end

        failure_message_when_negated do
            "Expected stderr output...\n#{@captured}\nto not match RE - #{expected}"
        end
    end

    let(:mockfs) { m = double("fuse"); m.stub(:getattr).and_return(nil); m }
    let(:mountpoint) { tempmount() }

    describe ".main" do

        let(:re_usage) { Regexp.new("^Usage:\n.*-h.*-d.*\n",Regexp::MULTILINE)  }
        let(:re_help)  { Regexp.new("^Fuse options: \\(\\d.\\d\\)\n.*help.*\n.*debug.*\n\n",Regexp::MULTILINE) }
        let(:re_fuse)  { Regexp.new(".*(^\\s+-o.*$)+.*",Regexp::MULTILINE) }
        let(:re_extra_header) { Regexp.new("Filesystem options:\n",Regexp::MULTILINE) }

        # self.main(argv=ARGV,extra_options=[],extra_option_usage="",device=nil,exec=File.basename($0))
        context "with no mountpoint" do
            it "prints usage information to stdout" do
                expect { RFuse.main([]) { } }.to output(re_usage).to_stdout
            end
            it "prints mountpoint failure on stderr" do
                expect { RFuse.main([]) { } }.to output("rfuse: failed, no mountpoint provided\n").to_stderr
            end
        end

        context "with help option" do
            it "does not start filesystem" do
                expect(mockfs).to receive(:init).exactly(0).times
                RFuse.main([mountpoint,"-h"]) { mockfs }
            end

            it "prints usage and kernel options" do
                # TODO: In Fuse 3.0, this will get complicated because help output moves to stdout
                re = Regexp.new(re_help.to_s + re_fuse.to_s)
                expect { RFuse.main([mountpoint,"-h"]) { mockfs } }.to print_to_stderr(re)
            end

            it "does not print filesystem options header if no extra options" do
                re = Regexp.new(re_help.to_s + re_fuse.to_s + re_extra_header.to_s)
                expect { RFuse.main([mountpoint,"-h"]) { mockfs } }.not_to print_to_stderr(re)
            end

            it "prints local option header if there are local options" do
                re = Regexp.new(re_help.to_s + re_fuse.to_s + re_extra_header.to_s + "TestOptionUsage")
                expect { RFuse.main([mountpoint,"-h"],[:myoption],"TestOptionUsage") { mockfs } }.to print_to_stderr(re)
            end

        end

        it "yields parsed options and cleaned argv" do
            # We know that main uses parse_options which is tested elsewhere
            expect { |b| RFuse.main([mountpoint,"-o","myoption"],[:myoption],&b) }.to \
                yield_with_args(a_hash_including(:mountpoint,:myoption),[mountpoint])
        end

        context "yield raises exception" do
            it "prints usage output" do
                expect { RFuse.main([mountpoint]) { raise RFuse::Error, "MyError"}}.to output(re_usage).to_stdout
            end

            it "prints the error message" do
                expect { RFuse.main([mountpoint]) { raise RFuse::Error, "MyError"}}.to output("rfuse failed: MyError\n").to_stderr
            end
        end

        it "creates and runs the yielded Fuse filesystem" do
            # We test create and run separately
            # expect RFuse.create then mounted? and run
            fuse = double(fuse)
            fuse.stub(:mounted?).and_return(true,false)
            expect(fuse).to receive(:run)
            expect(RFuse).to receive(:create).with(fuse,[mountpoint],{:mountpoint => mountpoint},[]) { fuse }
            RFuse.main([mountpoint]) { fuse }
        end
    end

    describe ".create" do
        let(:argv) { [mountpoint] }
        context "with a Fuse object" do
            let(:fs) { MockFuse.new(mountpoint) }
            it "returns the object" do
                fuse = RFuse.create(fs)
                expect(fuse).to be(fs)
                expect(fuse).to be_mounted
            end
        end

        context "with a subclass of Fuse" do
            let(:fs) { MockFuse }

            it "creates a new Fuse object from subclass of Fuse" do
                fuse = RFuse.create(fs,[mountpoint])
                expect(fuse).to be_kind_of(MockFuse)
                expect(fuse.mountpoint).to eq(mountpoint)
                expect(fuse).to be_mounted
            end
        end

        context "with a Class" do
            let(:fs) { DuckFuse }
            it "creates a FuseDelegator around a new instance" do
                expect(fs).to receive(:new).with("OptionValue").and_call_original()
                expect(RFuse::FuseDelegator).to receive(:new).with(an_instance_of(DuckFuse),mountpoint).and_call_original()
                fuse = RFuse.create(fs,[mountpoint],{:myopt => "OptionValue"},[:myopt])
                expect(fuse).to be_kind_of(RFuse::FuseDelegator)
                expect(fuse.mountpoint).to eq(mountpoint)
                expect(fuse).to be_mounted
            end
        end
        context "with a non-Fuse Object" do
            let(:fs) { mockfs }
            it "starts FuseDelegator with non Fuse object returned from yield" do
                expect(RFuse::FuseDelegator).to receive(:new).with(mockfs,mountpoint).and_call_original()
                fuse = RFuse.create(fs,[mountpoint])
                expect(fuse).to be_kind_of(RFuse::FuseDelegator)
                expect(fuse.mountpoint).to eq(mountpoint)
                expect(fuse).to be_mounted
            end
        end

    end
end
