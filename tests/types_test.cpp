// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/types.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "typesafe/testing.h"

namespace
{

using namespace typesafe;

TEST(ChoiceSerialization, WritesTypeAndCriteria)
{
	const Choice question{"What is the tone?",
	                      {{"angry", "An upset or hostile message"},
	                       {"calm", nlohmann::json{{"note", "polite"}}}}};

	const nlohmann::json wire = question;

	EXPECT_EQ(wire.at("type"), "choice");
	EXPECT_EQ(wire.at("criteria").at("angry"), "An upset or hostile message");
	EXPECT_TRUE(wire.at("criteria").at("calm").is_object());
}

TEST(ChoiceSerialization, OmitsInstructionsWhenAbsent)
{
	const nlohmann::json wire = Choice{std::nullopt, {{"yes", std::nullopt}}};

	EXPECT_FALSE(wire.contains("instructions"));
}

TEST(ScoreSerialization, KeepsCriteriaOrder)
{
	const nlohmann::json wire
	    = Score{"How urgent?",
	            {"can wait", "this week", nlohmann::json{{"label", "today"}}}};

	EXPECT_EQ(wire.at("type"), "score");
	EXPECT_TRUE(wire.at("criteria").is_array());
	EXPECT_EQ(wire.at("criteria").size(), 3u);
	EXPECT_EQ(wire.at("criteria").at(1), "this week");
}

TEST(NoulSerialization, WritesCriteriaUnderTrueAndFalse)
{
	const nlohmann::json wire = Noul{
	    std::nullopt, NoulCriteria{nlohmann::json("unsolicited advertising"),
		                           nlohmann::json("a real conversation")}};

	EXPECT_EQ(wire.at("type"), "noul");
	EXPECT_EQ(wire.at("criteria").at("true"), "unsolicited advertising");
	EXPECT_EQ(wire.at("criteria").at("false"), "a real conversation");
}

ScoreAnswer Decoded(nlohmann::json answer)
{
	return (answer.get<ScoreAnswer>());
}

TEST(ScoreAnswerDecoding, ReadsStructuredLegendAndProbabilities)
{
	const ScoreAnswer answer = Decoded({
	    {"type", "score"},
	    {"score", 1.7},
	    {"confidence", 0.9},
	    {"legend", {{"0", "bad"}, {"1", nlohmann::json{{"label", "ok"}}}}},
	    {"probabilities", {{"0", 0.1}, {"1", 0.9}}},
	});

	EXPECT_DOUBLE_EQ(answer.score, 1.7);
	EXPECT_DOUBLE_EQ(answer.confidence, 0.9);
	EXPECT_EQ(answer.legend.at(0), "bad");
	EXPECT_EQ(answer.legend.at(1).at("label"), "ok");
	EXPECT_DOUBLE_EQ(answer.probabilities.at(1), 0.9);
}

TEST(ScoreAnswerDecoding, RequiresEveryMandatoryField)
{
	EXPECT_THROW(Decoded({{"type", "score"}, {"score", 1.7}}),
	             nlohmann::json::exception);
	EXPECT_THROW(Decoded({{"type", "score"},
	                      {"score", 1.7},
	                      {"confidence", 0.9},
	                      {"probabilities", {{"0", 1.0}}}}),
	             nlohmann::json::exception);
}

TEST(ScoreAnswerDecoding, RejectsNonIntegerLegendKeys)
{
	EXPECT_THROW(Decoded({{"type", "score"},
	                      {"score", 1.7},
	                      {"confidence", 0.9},
	                      {"legend", {{"zero", "bad"}}},
	                      {"probabilities", {{"0", 1.0}}}}),
	             nlohmann::json::exception);
}

TEST(ScoreAnswerDecoding, RejectsValuesTheSchemaDoesNotAllow)
{
	EXPECT_THROW(Decoded({{"type", "score"},
	                      {"score", 1.7},
	                      {"confidence", 0.9},
	                      {"legend", {{"0", 5}}},
	                      {"probabilities", {{"0", 1.0}}}}),
	             nlohmann::json::exception);
}

TEST(ResponseDecoding, DispatchesEachAnswerByType)
{
	const SystemOneResponse response = nlohmann::json::parse(R"json({
	    "model": "jev-1.13.0",
	    "usage": {"input_tokens": 1, "output_tokens": 2},
	    "answers": {
	        "billing": {"type": "noul", "noul": 0.98},
	        "tone": {
	            "type": "choice",
	            "choice": "angry",
	            "confidence": 0.8,
	            "probabilities": {"angry": 0.8, "calm": 0.2}
	        },
	        "urgency": {
	            "type": "score",
	            "score": 2.0,
	            "confidence": 0.7,
	            "legend": {"0": "can wait", "1": "soon", "2": "now"},
	            "probabilities": {"0": 0.0, "1": 0.2, "2": 0.8}
	        }
	    }
	})json")
	                                       .get<SystemOneResponse>();

	EXPECT_EQ(response.model, "jev-1.13.0");
	EXPECT_EQ(response.usage.input_tokens, 1);
	EXPECT_DOUBLE_EQ(response.nouls.at("billing").noul, 0.98);
	EXPECT_EQ(response.choices.at("tone").choice, "angry");
	EXPECT_DOUBLE_EQ(response.choices.at("tone").confidence, 0.8);
	EXPECT_DOUBLE_EQ(response.scores.at("urgency").score, 2.0);
}

TEST(ResponseDecoding, RejectsAnEmptyAnswersObject)
{
	const nlohmann::json response{
	    {"model", "jev-1.13.0"},
	    {"usage", {{"input_tokens", 1}, {"output_tokens", 2}}},
	    {"answers", nlohmann::json::object()}};

	EXPECT_THROW(response.get<SystemOneResponse>(), nlohmann::json::exception);
}

TEST(ResponseDecoding, RejectsAMissingAnswersObject)
{
	const nlohmann::json response{
	    {"model", "jev-1.13.0"},
	    {"usage", {{"input_tokens", 1}, {"output_tokens", 2}}}};

	EXPECT_THROW(response.get<SystemOneResponse>(), nlohmann::json::exception);
}

}  // namespace
